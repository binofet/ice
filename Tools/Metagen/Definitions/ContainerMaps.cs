using Metagen.PortableData;
using Metagen;

[assembly: ContainerMap(typeof(List<>), ContainerShape.Sequence, TargetProfile.Cpp20,
    "vector", "std::vector<{0}>", "metagen_examples::vector_adapter")]
[assembly: ContainerMap(typeof(List<>), ContainerShape.Sequence, TargetProfile.Cpp23,
    "playground/custom_vector.h", "playground::custom_vector<{0}>", "playground::vector_adapter")]
[assembly: ContainerMap(typeof(Dictionary<,>), ContainerShape.Map, TargetProfile.Cpp20,
    "map", "std::map<{0}, {1}>", "metagen_examples::map_adapter")]
[assembly: ContainerMap(typeof(List<>), ContainerShape.Sequence, TargetProfile.Rust2021,
    "std/vec", "Vec<{0}>", "metagen::sequence_adapter")]
