# BlueprintJson

A standalone .NET console tool that converts **uncooked Unreal Engine editor Blueprint
`.uasset` files** into a compact, token-efficient JSON projection of their node graphs —
and performs narrow, surgical write-backs of pin default values. Built on
[UAssetAPI](https://github.com/atenfyr/UAssetAPI).

UAssetAPI parses tagged properties but leaves the editor-only `UEdGraphPin` array
(custom binary appended by `UEdGraphNode::Serialize`) as opaque `Extras` bytes.
`PinBlob.cs` implements that wire format (transcribed from the UE 5.6 sources:
`EdGraphPin.cpp` / `EdGraphNode.cpp`), including `FEdGraphPinType`,
`FSimpleMemberReference`, `FEdGraphTerminalType`, and header-less `FText` bodies.

## Setup

```sh
brew install dotnet            # if you don't have a .NET SDK (needs net10.0)
cd Tools/BlueprintJson
git clone https://github.com/atenfyr/UAssetAPI external/UAssetAPI   # if missing
dotnet build
```

## Usage

```sh
# Dump a Blueprint to JSON (stdout, or a file if given)
dotnet run -- dump ../../Content/Blueprints/LibretroTVPawn.uasset tvpawn.json

# Change a pin's DefaultValue in place (node guid from the dump; pin by name or guid)
dotnet run -- set-default copy.uasset "{C3DAB3FD-9045-7E9B-1C87-898B32ACFCEE}" PeerId 42

# Verify UAssetAPI can resave the asset byte-for-byte (per-asset write-path trust check)
dotnet run -- roundtrip-check ../../Content/Blueprints/LibretroHUD.uasset
```

## Output shape

```json
{
  "asset": "LibretroFocusViewPawn.uasset",
  "savedByEngine": "4.25.4",
  "graphs": [{ "name": "EventGraph", "nodes": [{
      "id": "N276", "class": "K2Node_InputKey", "name": "K2Node_InputKey_1",
      "guid": "{7884AC39-48F4-A411-999C-FF94394C346F}", "pos": [1392, 944],
      "props": { "InputKey": { "KeyName": "Colon" } },
      "pins": [
        { "name": "Pressed", "dir": "out", "type": "exec", "links": ["N255.execute"] },
        { "name": "Key", "dir": "out", "type": "struct<Key>", "default": "None" }
      ]
  }]}],
  "components": [ { "name": "Screen", "class": "StaticMeshComponent", "templateProps": {...}, "children": [...] } ],
  "classDefaults": { "name": "Default__LibretroTVPawn_C", "props": {...} }
}
```

- `id` is `N<packageExportIndex>`; pin link targets are flattened to `N<idx>.<pinName>`.
- `type` is a compact rendering of `FEdGraphPinType`, e.g. `object<PlayerController>`,
  `real:double`, `array<struct<Vector>>`, `delegate(=OnFrame__DelegateSignature)`, `int&`.
- Hidden/advanced/orphaned pins are flagged; noisy editor-layout node properties are omitted.
- `extraSerializedBytes` on a node marks trailing subclass payload after the pin array
  (e.g. `UK2Node_EditablePinBase` user-defined pins on FunctionEntry/CustomEvent/Tunnel
  nodes) that the tool preserves verbatim but does not decode.

## Write-back semantics

`set-default` re-parses the node's pin blob to locate the exact byte range of the pin's
`DefaultValue` FString inside `Extras`, splices in the new value (ANSI or UTF-16 encoded
exactly as UE would), and resaves through UAssetAPI. No topology, no other bytes touched.
It then reloads the saved asset and re-parses to verify. Run `roundtrip-check` on an asset
first: if it passes (all 9 assets in `Content/Blueprints/` do), the resave path is
byte-perfect and the only bytes that change are the spliced default.

Caveats:
- Don't edit defaults of *user-defined* pins on FunctionEntry/CustomEvent nodes this way —
  those duplicate their defaults inside the undecoded `UK2Node_EditablePinBase` payload.
- The Unreal editor holds advisory locks on open assets; reads work anyway (the tool opens
  with shared access), but don't write to an asset while the editor has it open.
- Assets are versioned; both UE 4.25-era and UE 5.6-saved packages parse. The engine-version
  hint is sniffed from the header because UAssetAPI mis-parses UE4-era headers when handed
  a UE5 hint (a preset `ObjectVersionUE5` is never cleared for files with
  `LegacyFileVersion > -8`).
