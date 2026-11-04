# Isolated COL v8 converter

Offline preparation tool; not a game runtime. Targets the already installed .NET 9 SDK, uses only BCL, and restores no NuGet packages.

`ColFile.cs`, `ColFace.cs`, `ColObject.cs`, `ColScene.cs` are unmodified MIT-licensed sources from https://github.com/slfx77/neversoft-multitool commit `3c9028e0178deb9060190362d1c47039a32846e2`. See `UPSTREAM-LICENSE.md`. `Program.cs` and the project file are local integration code.

Build: `dotnet build Col8Converter.csproj -c Release`

Run: `dotnet bin/Release/net9.0/Col8Converter.dll input.col.ps2 output.dscollision`

Outputs: `output.dscollision` and `output.dscollision.json`. The binary has a 12-byte header (8-byte ASCII `DSCOL001`, little-endian u32 triangle count), followed by 44 bytes per triangle: nine float32 position values, u16 original face flags, u16 terrain, u32 source object checksum. Object flags and source provenance are retained in the JSON. Coordinates and flags are not transformed. No collision-policy meaning is inferred.

The upstream parser validates object/vertex/face partitions, indices, finite coordinates and original BSP structure. An unsupported layout is an error, not silently replaced by guessed geometry.
