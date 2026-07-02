// BlueprintJson - dump uncooked UE 5.6 Blueprint .uasset files to a compact JSON
// projection of their node graphs (and write back pin default values).
// Built on UAssetAPI (github.com/atenfyr/UAssetAPI).

using Newtonsoft.Json;
using Newtonsoft.Json.Linq;
using UAssetAPI;
using UAssetAPI.ExportTypes;
using UAssetAPI.PropertyTypes.Objects;
using UAssetAPI.PropertyTypes.Structs;
using UAssetAPI.UnrealTypes;

namespace BlueprintJson;

public static class Program
{
    static readonly EngineVersion Engine = EngineVersion.VER_UE5_6;

    public static int Main(string[] args)
    {
        try
        {
            switch (args.FirstOrDefault())
            {
                case "dump" when args.Length is 2 or 3:
                    return Dump(args[1], args.Length == 3 ? args[2] : null);
                case "set-default" when args.Length == 5:
                    return SetDefault(args[1], args[2], args[3], args[4]);
                case "roundtrip-check" when args.Length == 2:
                    return RoundtripCheck(args[1]);
                default:
                    Console.Error.WriteLine(
                        "usage: BlueprintJson dump <asset.uasset> [out.json]\n" +
                        "       BlueprintJson set-default <asset.uasset> <nodeGuid> <pinName|pinGuid> <newDefaultValue>\n" +
                        "       BlueprintJson roundtrip-check <asset.uasset>");
                    return 2;
            }
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine($"error: {ex.Message}");
            return 1;
        }
    }

    // Loads an asset, tolerating advisory file locks held by a running Unreal editor
    // (we open with FileShare.ReadWrite and parse from memory). The engine-version hint
    // is sniffed from the header: UAssetAPI mis-parses UE4-era files when given a UE5
    // hint because a preset ObjectVersionUE5 is never cleared for files that lack a UE5
    // file version (LegacyFileVersion > -8).
    static UAsset Load(string path) => Load(path, out _);

    static UAsset Load(string path, out byte[] originalBytes)
    {
        using var fs = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete);
        var bytes = new byte[fs.Length];
        fs.ReadExactly(bytes);
        originalBytes = bytes;

        var hint = bytes.Length >= 8 && BitConverter.ToInt32(bytes, 4) <= -8 ? Engine : EngineVersion.VER_UE4_27;
        var asset = new UAsset(hint) { FilePath = path };
        asset.Read(new AssetBinaryReader(new MemoryStream(bytes), asset));
        return asset;
    }

    // ---------------------------------------------------------------- dump

    static int Dump(string path, string outPath)
    {
        var asset = Load(path);
        var root = BuildDump(asset, path);
        string json = root.ToString(Formatting.Indented);
        if (outPath != null) { File.WriteAllText(outPath, json); Console.WriteLine($"wrote {outPath} ({json.Length} chars)"); }
        else Console.WriteLine(json);
        return 0;
    }

    static JObject BuildDump(UAsset asset, string path)
    {
        // Collect every EdGraph export and the node exports it references.
        var graphs = new List<(NormalExport graph, List<int> nodeIndices)>();
        for (int i = 0; i < asset.Exports.Count; i++)
        {
            if (asset.Exports[i] is not NormalExport ne) continue;
            if (ne.GetExportClassType()?.ToString() != "EdGraph") continue;
            var nodes = new List<int>();
            if (FindProp<ArrayPropertyData>(ne, "Nodes") is { } arr)
                foreach (var e in arr.Value)
                    if (e is ObjectPropertyData op && op.Value.IsExport()) nodes.Add(op.Value.Index);
            graphs.Add((ne, nodes));
        }

        // Parse pins for every node export up front so links can be resolved to pin names.
        var pinsByExport = new Dictionary<int, NodePins>();                      // package index -> pins
        var pinName = new Dictionary<(int, Guid), string>();                     // (node package index, pin id) -> name
        foreach (var (_, nodeIndices) in graphs)
            foreach (int idx in nodeIndices)
            {
                if (pinsByExport.ContainsKey(idx)) continue;
                var np = PinBlob.Parse(asset, asset.Exports[idx - 1].Extras);
                pinsByExport[idx] = np;
                foreach (var p in np.Pins) pinName[(idx, p.PinId)] = p.PinName;
            }

        var jGraphs = new JArray();
        foreach (var (graph, nodeIndices) in graphs)
        {
            var jNodes = new JArray();
            foreach (int idx in nodeIndices)
                jNodes.Add(NodeToJson(asset, idx, pinsByExport[idx], pinName));
            jGraphs.Add(new JObject { ["name"] = graph.ObjectName.ToString(), ["nodes"] = jNodes });
        }

        var root = new JObject
        {
            ["asset"] = Path.GetFileName(path),
            ["savedByEngine"] = $"{asset.RecordedEngineVersion.Major}.{asset.RecordedEngineVersion.Minor}.{asset.RecordedEngineVersion.Patch}",
            ["graphs"] = jGraphs,
        };
        if (BuildScs(asset) is { } scs) root["components"] = scs;
        if (BuildCdo(asset) is { } cdo) root["classDefaults"] = cdo;
        return root;
    }

    // Node tagged properties that are pure editor/visual noise in a dump.
    static readonly HashSet<string> NoiseProps = new()
    {
        "NodeGuid", "NodePosX", "NodePosY", "NodeWidth", "NodeHeight", "NodeComment",
        "bCommentBubblePinned", "bCommentBubbleVisible", "bCommentBubbleVisible_InDetailsPanel",
        "ErrorType", "ErrorMsg", "AdvancedPinDisplay", "EnabledState", "bUserSetEnabledState",
        "CommentColor", "FontSize", "bColorCommentBubble", "MoveMode", "CommentDepth",
    };

    static JObject NodeToJson(UAsset asset, int pkgIndex, NodePins np, Dictionary<(int, Guid), string> pinName)
    {
        var ex = (NormalExport)asset.Exports[pkgIndex - 1];
        var j = new JObject
        {
            ["id"] = $"N{pkgIndex}",
            ["class"] = ex.GetExportClassType()?.ToString(),
            ["name"] = ex.ObjectName.ToString(),
        };
        if (GetNodeGuid(ex) is { } g) j["guid"] = g.ConvertToString();
        int? x = (FindProp<IntPropertyData>(ex, "NodePosX"))?.Value;
        int? y = (FindProp<IntPropertyData>(ex, "NodePosY"))?.Value;
        if (x.HasValue || y.HasValue) j["pos"] = new JArray(x ?? 0, y ?? 0);
        if (FindProp<StrPropertyData>(ex, "NodeComment")?.Value?.Value is { } comment) j["comment"] = comment;

        var props = new JObject();
        foreach (var p in ex.Data)
            if (!NoiseProps.Contains(p.Name.ToString()))
                props[p.Name.ToString()] = PropToken(asset, p);
        if (props.Count > 0) j["props"] = props;

        if (np.Error != null) j["pinParseError"] = np.Error;
        var jPins = new JArray();
        foreach (var pin in np.Pins)
        {
            var jp = new JObject
            {
                ["name"] = pin.PinName,
                ["dir"] = pin.Direction == 1 ? "out" : "in",
                ["type"] = TypeStr(asset, pin.Type),
            };
            string def = pin.DefaultObject.Index != 0 ? RefStr(asset, pin.DefaultObject)
                       : !string.IsNullOrEmpty(pin.DefaultTextValue) ? pin.DefaultTextValue
                       : pin.DefaultValue;
            if (!string.IsNullOrEmpty(def)) jp["default"] = def;
            if (pin.LinkedTo.Count > 0)
                jp["links"] = new JArray(pin.LinkedTo.Select(l => LinkStr(l, pinName)));
            if (pin.ParentPin is { IsNull: false } pp) jp["parent"] = LinkPinName(pp, pinName);
            if (pin.Hidden) jp["hidden"] = true;
            if (pin.AdvancedView) jp["advanced"] = true;
            if (pin.Orphaned) jp["orphaned"] = true;
            jPins.Add(jp);
        }
        j["pins"] = jPins;
        if (np.TrailingBytes > 0) j["extraSerializedBytes"] = np.TrailingBytes;
        return j;
    }

    static string LinkStr(PinRef l, Dictionary<(int, Guid), string> pinName)
    {
        if (l.IsNull) return null;
        return $"N{l.OwningNode.Index}.{LinkPinName(l, pinName)}";
    }

    static string LinkPinName(PinRef l, Dictionary<(int, Guid), string> pinName) =>
        pinName.TryGetValue((l.OwningNode.Index, l.PinId), out var n) ? n : l.PinId.ConvertToString();

    static string TypeStr(UAsset asset, PinTypeInfo t)
    {
        if (t == null) return null;
        string core = t.Category ?? "?";
        string obj = RefStr(asset, t.SubCategoryObject);
        if (!string.IsNullOrEmpty(obj)) core += $"<{obj}>";
        else if (t.SubCategory is { Length: > 0 } sub && sub != "None") core += $":{sub}";
        if (t.MemberName is { Length: > 0 } mn && mn != "None") core += $"(={mn})";
        core = t.ContainerType switch
        {
            1 => $"array<{core}>",
            2 => $"set<{core}>",
            3 => $"map<{core},{TermStr(asset, t)}>",
            _ => core,
        };
        if (t.IsReference) core += "&";
        return core;
    }

    static string TermStr(UAsset asset, PinTypeInfo t)
    {
        string core = t.TermCategory ?? "?";
        string obj = RefStr(asset, t.TermSubCategoryObject);
        if (!string.IsNullOrEmpty(obj)) core += $"<{obj}>";
        else if (t.TermSubCategory is { Length: > 0 } sub && sub != "None") core += $":{sub}";
        return core;
    }

    // ------------------------------------------------- SCS component tree

    static JArray BuildScs(UAsset asset)
    {
        NormalExport scs = asset.Exports.OfType<NormalExport>()
            .FirstOrDefault(e => e.GetExportClassType()?.ToString() == "SimpleConstructionScript");
        if (scs == null) return null;
        if (FindProp<ArrayPropertyData>(scs, "RootNodes") is not { } roots) return null;
        var arr = new JArray();
        foreach (var e in roots.Value)
            if (e is ObjectPropertyData op && op.Value.IsExport())
                arr.Add(ScsNodeToJson(asset, op.Value.Index));
        return arr;
    }

    static JObject ScsNodeToJson(UAsset asset, int pkgIndex)
    {
        var ex = (NormalExport)asset.Exports[pkgIndex - 1];
        var j = new JObject
        {
            ["name"] = FindProp<NamePropertyData>(ex, "InternalVariableName")?.Value?.ToString() ?? ex.ObjectName.ToString(),
            ["class"] = RefStr(asset, FindProp<ObjectPropertyData>(ex, "ComponentClass")?.Value),
        };
        // Template property overrides (cheap: template export's tagged properties)
        if (FindProp<ObjectPropertyData>(ex, "ComponentTemplate")?.Value is { } tmpl && tmpl.IsExport()
            && asset.Exports[tmpl.Index - 1] is NormalExport te && te.Data.Count > 0)
        {
            var props = new JObject();
            foreach (var p in te.Data) props[p.Name.ToString()] = PropToken(asset, p);
            j["templateProps"] = props;
        }
        if (FindProp<ArrayPropertyData>(ex, "ChildNodes") is { } childs && childs.Value.Length > 0)
        {
            var arr = new JArray();
            foreach (var e in childs.Value)
                if (e is ObjectPropertyData op && op.Value.IsExport())
                    arr.Add(ScsNodeToJson(asset, op.Value.Index));
            j["children"] = arr;
        }
        return j;
    }

    // -------------------------------------------------------- CDO overrides

    static JObject BuildCdo(UAsset asset)
    {
        NormalExport cdo = asset.Exports.OfType<NormalExport>()
            .FirstOrDefault(e => e.ObjectFlags.HasFlag(EObjectFlags.RF_ClassDefaultObject));
        if (cdo == null || cdo.Data.Count == 0) return null;
        var props = new JObject();
        foreach (var p in cdo.Data) props[p.Name.ToString()] = PropToken(asset, p);
        return new JObject { ["name"] = cdo.ObjectName.ToString(), ["props"] = props };
    }

    // ----------------------------------------------- property -> JSON token

    static JToken PropToken(UAsset asset, PropertyData p)
    {
        switch (p)
        {
            case BoolPropertyData b: return b.Value;
            case IntPropertyData i: return i.Value;
            case Int64PropertyData i: return i.Value;
            case UInt32PropertyData i: return i.Value;
            case FloatPropertyData f: return f.Value;
            case DoublePropertyData d: return d.Value;
            case StrPropertyData s: return s.Value?.Value;
            case NamePropertyData n: return n.Value?.ToString();
            case TextPropertyData t: return t.CultureInvariantString?.Value ?? t.Value?.Value;
            case EnumPropertyData e: return e.Value?.ToString();
            case BytePropertyData by:
                return by.ByteType == BytePropertyType.FName ? by.EnumValue?.ToString() : by.Value.ToString();
            case ObjectPropertyData o: return RefStr(asset, o.Value);
            case SoftObjectPropertyData so: return so.Value.ToString();
            case GuidPropertyData g: return g.Value.ConvertToString();
            case VectorPropertyData v: return new JArray(v.Value.X, v.Value.Y, v.Value.Z);
            case Vector2DPropertyData v: return new JArray(v.Value.X, v.Value.Y);
            case RotatorPropertyData r: return new JArray(r.Value.Pitch, r.Value.Yaw, r.Value.Roll);
            case LinearColorPropertyData c: return new JArray(c.Value.R, c.Value.G, c.Value.B, c.Value.A);
            case StructPropertyData st:
            {
                var o = new JObject();
                foreach (var f in st.Value) o[f.Name.ToString()] = PropToken(asset, f);
                return o;
            }
            case ArrayPropertyData a: return new JArray(a.Value.Select(e => PropToken(asset, e))); // also covers SetPropertyData
            default:
                try { return p.ToString(); } catch { return $"<{p.PropertyType}>"; }
        }
    }

    static string RefStr(UAsset asset, FPackageIndex idx)
    {
        if (idx == null || idx.Index == 0) return null;
        if (idx.IsImport()) return idx.ToImport(asset).ObjectName.ToString();
        return asset.Exports[idx.Index - 1].ObjectName.ToString();
    }

    static T FindProp<T>(NormalExport ex, string name) where T : PropertyData =>
        ex.Data.FirstOrDefault(p => p.Name.ToString() == name) as T;

    static Guid? GetNodeGuid(NormalExport ex) =>
        FindProp<StructPropertyData>(ex, "NodeGuid")?.Value?.FirstOrDefault() is GuidPropertyData g ? g.Value : null;

    static string NormGuid(string s) => s.Replace("-", "").Replace("{", "").Replace("}", "").ToLowerInvariant();

    // --------------------------------------------------------- set-default

    static int SetDefault(string path, string nodeGuid, string pinKey, string newValue)
    {
        var asset = Load(path);
        string want = NormGuid(nodeGuid);

        NormalExport node = null;
        int nodeIdx = 0;
        for (int i = 0; i < asset.Exports.Count; i++)
            if (asset.Exports[i] is NormalExport ne && GetNodeGuid(ne) is { } g && NormGuid(g.ConvertToString()) == want)
            { node = ne; nodeIdx = i + 1; break; }
        if (node == null) { Console.Error.WriteLine($"no node with guid {nodeGuid}"); return 1; }

        var np = PinBlob.Parse(asset, node.Extras);
        if (np.Error != null) { Console.Error.WriteLine($"pin parse failed for {node.ObjectName}: {np.Error}"); return 1; }

        var pin = np.Pins.FirstOrDefault(p =>
            string.Equals(p.PinName, pinKey, StringComparison.OrdinalIgnoreCase) ||
            NormGuid(p.PinId.ConvertToString()) == NormGuid(pinKey));
        if (pin == null)
        {
            Console.Error.WriteLine($"no pin '{pinKey}' on node {node.ObjectName}; pins: " +
                string.Join(", ", np.Pins.Select(p => p.PinName)));
            return 1;
        }

        // Surgical splice: replace only the DefaultValue FString bytes inside Extras.
        byte[] extras = node.Extras;
        byte[] encoded = PinBlob.EncodeFString(newValue);
        byte[] newExtras = new byte[pin.DefaultValueStart + encoded.Length + (extras.Length - pin.DefaultValueEnd)];
        Buffer.BlockCopy(extras, 0, newExtras, 0, pin.DefaultValueStart);
        Buffer.BlockCopy(encoded, 0, newExtras, pin.DefaultValueStart, encoded.Length);
        Buffer.BlockCopy(extras, pin.DefaultValueEnd, newExtras, pin.DefaultValueStart + encoded.Length, extras.Length - pin.DefaultValueEnd);
        node.Extras = newExtras;

        asset.Write(path);
        Console.WriteLine($"set N{nodeIdx} ({node.ObjectName}) pin '{pin.PinName}' default: '{pin.DefaultValue}' -> '{newValue}'");

        // Verify by re-reading the saved asset.
        var check = Load(path);
        var checkNode = (NormalExport)check.Exports[nodeIdx - 1];
        var checkPins = PinBlob.Parse(check, checkNode.Extras);
        var checkPin = checkPins.Pins.FirstOrDefault(p => p.PinId == pin.PinId);
        bool ok = checkPins.Error == null && checkPin != null && (checkPin.DefaultValue ?? "") == newValue;
        Console.WriteLine(ok ? "verified: reloaded asset parses and pin has new value"
                             : $"VERIFY FAILED: {checkPins.Error ?? ("value is '" + checkPin?.DefaultValue + "'")}");
        return ok ? 0 : 1;
    }

    // ------------------------------------------------------ roundtrip-check

    static int RoundtripCheck(string path)
    {
        var asset = Load(path, out byte[] original);
        bool equal = asset.WriteData().ToArray().AsSpan().SequenceEqual(original);
        Console.WriteLine($"{Path.GetFileName(path)}: {(equal ? "PASS - resave is binary-identical" : "FAIL - resave differs from original")}");
        return equal ? 0 : 1;
    }
}
