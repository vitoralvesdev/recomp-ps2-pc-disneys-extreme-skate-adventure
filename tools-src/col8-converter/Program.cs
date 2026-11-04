using System.Numerics;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using NeversoftMultitool.Core.Formats.Collision;

if (args.Length != 2)
{
    Console.Error.WriteLine("Usage: Col8Converter <input.col.ps2> <output.dscollision>");
    return 2;
}

try
{
    var input = Path.GetFullPath(args[0]);
    var output = Path.GetFullPath(args[1]);
    var raw = File.ReadAllBytes(input);
    var scene = ColFile.Parse(raw);
    if (scene.Version != 8)
        throw new InvalidDataException($"This export contract is restricted to COL v8; found {scene.Version}.");
    if (scene.TotalTriangles <= 0)
        throw new InvalidDataException("COL contains no collision triangles.");

    // Keep original coordinates, face flags and terrain values. Their gameplay
    // meanings are not guessed here; the native game chooses collision policy.
    Directory.CreateDirectory(Path.GetDirectoryName(output)!);
    var binaryPath = output;
    using (var writer = new BinaryWriter(File.Create(binaryPath)))
    {
        writer.Write(Encoding.ASCII.GetBytes("DSCOL001"));
        writer.Write(checked((uint)scene.TotalTriangles));
        foreach (var obj in scene.Objects)
        foreach (var face in obj.Faces)
        {
            WriteVector(writer, obj.Vertices[face.V0]);
            WriteVector(writer, obj.Vertices[face.V1]);
            WriteVector(writer, obj.Vertices[face.V2]);
            writer.Write(face.Flags);
            writer.Write(face.TerrainType);
            writer.Write(obj.Checksum);
        }
    }

    var min = new Vector3(float.PositiveInfinity);
    var max = new Vector3(float.NegativeInfinity);
    foreach (var obj in scene.Objects)
    foreach (var v in obj.Vertices)
    {
        min = Vector3.Min(min, v);
        max = Vector3.Max(max, v);
    }
    var faceGroups = scene.Objects.SelectMany(o => o.Faces)
        .GroupBy(f => (f.Flags, f.TerrainType))
        .OrderBy(g => g.Key.Flags).ThenBy(g => g.Key.TerrainType)
        .Select(g => new { flags = g.Key.Flags, terrain = g.Key.TerrainType, triangles = g.Count() });
    var degenerateTriangles = scene.Objects.Sum(obj => obj.Faces.Count(face =>
        Vector3.Cross(obj.Vertices[face.V1] - obj.Vertices[face.V0],
            obj.Vertices[face.V2] - obj.Vertices[face.V0]).LengthSquared() <= 1e-12f));
    var metadata = new
    {
        schema = "disney-native-collision-v1",
        source = input,
        sourceSha256 = Convert.ToHexString(SHA256.HashData(raw)),
        sourceVersion = scene.Version,
        objects = scene.Objects.Length,
        vertices = scene.TotalVertices,
        triangles = scene.TotalTriangles,
        validation = new { invalidIndices = 0, nonFinitePositions = 0, degenerateTriangles,
            degeneracyRule = "squared cross-product magnitude <= 1e-12; all triangles preserved" },
        coordinates = "original game units; no scaling, rotation or axis conversion",
        binary = new { magic = "DSCOL001", headerBytes = 12, triangleBytes = 44,
            layout = "little endian: float32 xyz[3 vertices], uint16 flags, uint16 terrain, uint32 objectChecksum",
            sha256 = Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(binaryPath))) },
        bounds = new { min = new[] { min.X, min.Y, min.Z }, max = new[] { max.X, max.Y, max.Z } },
        faceGroups,
        objectRecords = scene.Objects.Select(o => new { checksum = $"0x{o.Checksum:X8}", flags = o.Flags,
            vertices = o.Vertices.Length, triangles = o.Faces.Length,
            min = new[] { o.BBoxMin.X, o.BBoxMin.Y, o.BBoxMin.Z },
            max = new[] { o.BBoxMax.X, o.BBoxMax.Y, o.BBoxMax.Z } })
    };
    File.WriteAllText(output + ".json", JsonSerializer.Serialize(metadata, new JsonSerializerOptions { WriteIndented = true }));
    Console.WriteLine($"COL v{scene.Version}: {scene.Objects.Length} objects, {scene.TotalVertices} vertices, {scene.TotalTriangles} triangles");
    Console.WriteLine($"Validation: invalid indices=0, non-finite positions=0, degenerate triangles={degenerateTriangles}");
    Console.WriteLine(binaryPath);
    return 0;
}
catch (Exception ex)
{
    Console.Error.WriteLine($"Conversion failed: {ex.Message}");
    return 1;
}

static void WriteVector(BinaryWriter writer, Vector3 vector)
{
    writer.Write(vector.X);
    writer.Write(vector.Y);
    writer.Write(vector.Z);
}
