// Macro to suppress compiler-specific warnings
//THIRD_PARTY_INCLUDES_START
// Microsoft Visual C++ Compiler
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4125)  // decimal digit terminates octal escape sequence
#pragma warning(disable: 4456)  // declaration of 'variable' hides previous local declaration
#pragma warning(disable: 4510)  // default constructor could not be generated
#pragma warning(disable: 4610)  // object can never be instantiated - user-defined constructor required
#pragma warning(disable: 4668)
#pragma warning(disable: 4800)  // Implicit conversion from 'type' to bool. Possible information loss
#pragma warning(disable: 4946)  // reinterpret_cast used between related classes
#pragma warning(disable: 4996)  // 'item' was declared deprecated
#pragma warning(disable: 6011)  // Dereferencing NULL pointer
#pragma warning(disable: 6101)  // Returning uninitialized memory
#pragma warning(disable: 6287)  // Redundant code: the left and right sub-expressions are identical
#pragma warning(disable: 6308)  // 'realloc' might return null pointer
#pragma warning(disable: 6326)  // Potential comparison of a constant with another constant
#pragma warning(disable: 6340)  // Mismatch on sign: Incorrect type passed as parameter in call to function
#pragma warning(disable: 6385)  // Reading invalid data
#pragma warning(disable: 6386)  // Buffer overrun while writing to
#pragma warning(disable: 6553)  // The annotation for function does not apply to a value type
#pragma warning(disable: 28182) // Dereferencing NULL pointer
#pragma warning(disable: 28251) // Inconsistent annotation for function
#pragma warning(disable: 28252) // Inconsistent annotation for function
#pragma warning(disable: 28253) // Inconsistent annotation for function
#pragma warning(disable: 28301) // No annotations for first declaration of function

// GCC and Clang Compiler
#elif defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-value"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
// Add other relevant warning suppressions for GCC/Clang here

#ifdef __clang__
#pragma clang diagnostic ignored "-Wmismatched-tags"
#endif

// Fallback for other compilers
#else
// No known warning suppressions available for other compilers
#endif

// Considering all things this is somehow the most practical way to build external libraries in my mind lol
// This is basically just statically linking everything but in a roundabout fashion
// Static linking is the most practical thing to do since:
//   - We don't have to download dll's for each platform or use external build-tools
//   - We don't have to worry about loading the dll
//   - We aren't at risk of polluting the symbol-space
//   - Source code indexing still works in Visual Studio :D
// This may end up being unmaintainable in which case I'll do something more sane

// This is good practice but also necessary considering Unreal allows unity builds
#ifdef _MSC_VER
#pragma warning(pop)
#endif
