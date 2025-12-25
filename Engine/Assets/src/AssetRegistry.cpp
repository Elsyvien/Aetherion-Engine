#include "Aetherion/Assets/AssetRegistry.h"
#include "Aetherion/Core/String.h"
#include "Aetherion/Core/UUID.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <limits>
#include <random>
#include <regex>
#include <sstream>
#include <unordered_set>

#define CGLTF_IMPLEMENTATION
#include "cgltf/cgltf.h"

namespace {
using namespace Aetherion::Assets;

std::string ToLower(std::string value) {
  std::transform(
      value.begin(), value.end(), value.begin(),
      [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return value;
}

bool HasSuffix(const std::string &value, const std::string &suffix) {
  if (value.size() < suffix.size()) {
    return false;
  }
  return value.compare(value.size() - suffix.size(), suffix.size(), suffix) ==
         0;
}

bool PathHasSegment(const std::filesystem::path &path,
                    const std::string &segment) {
  for (const auto &part : path) {
    if (part == segment) {
      return true;
    }
  }
  return false;
}

bool IsMetadataPath(const std::filesystem::path &path) {
  const std::string filename = ToLower(path.filename().string());
  return HasSuffix(filename, ".asset.json");
}

std::filesystem::path
BuildMetadataPath(const std::filesystem::path &assetPath) {
  std::filesystem::path metaPath = assetPath;
  metaPath += ".asset.json";
  return metaPath;
}

bool ReadMetadataFile(const std::filesystem::path &metaPath, std::string &outId,
                      std::string *outSource, std::string *outType) {
  std::ifstream input(metaPath);
  if (!input.is_open()) {
    return false;
  }

  std::string content((std::istreambuf_iterator<char>(input)),
                      std::istreambuf_iterator<char>());
  input.close();

  std::smatch match;
  std::regex idRegex("\\\"id\\\"\\s*:\\s*\\\"([^\\\"]+)\\\"");
  if (std::regex_search(content, match, idRegex) && match.size() >= 2) {
    outId = match[1].str();
  } else {
    return false;
  }

  if (outSource) {
    std::regex sourceRegex("\\\"source\\\"\\s*:\\s*\\\"([^\\\"]+)\\\"");
    if (std::regex_search(content, match, sourceRegex) && match.size() >= 2) {
      *outSource = match[1].str();
    }
  }

  if (outType) {
    std::regex typeRegex("\\\"type\\\"\\s*:\\s*\\\"([^\\\"]+)\\\"");
    if (std::regex_search(content, match, typeRegex) && match.size() >= 2) {
      *outType = match[1].str();
    }
  }

  return true;
}

void WriteMetadataFile(const std::filesystem::path &metaPath,
                       const std::string &id, AssetRegistry::AssetType type,
                       const std::string &source) {
  std::ofstream output(metaPath, std::ios::trunc);
  if (!output.is_open()) {
    return;
  }

  output << "{\n";
  output << "  \"version\": 1,\n";
  output << "  \"id\": \"" << id << "\",\n";
  output << "  \"type\": \"" << AssetRegistry::AssetTypeToString(type)
         << "\",\n";
  output << "  \"source\": \"" << source << "\"\n";
  output << "}\n";
  output.close();
}

bool EnsureMetadataForAsset(const std::filesystem::path &assetPath,
                            const std::filesystem::path &rootPath,
                            AssetRegistry::AssetType type, std::string &outId) {
  std::error_code ec;
  std::filesystem::path relative =
      std::filesystem::relative(assetPath, rootPath, ec);
  std::string sourceLabel = (!ec && !relative.empty())
                                ? relative.generic_string()
                                : assetPath.filename().generic_string();

  const std::filesystem::path metaPath = BuildMetadataPath(assetPath);
  std::string existingId;
  std::string metaSource;
  std::string metaType;
  bool writeMeta = false;

  if (std::filesystem::exists(metaPath, ec)) {
    if (!ReadMetadataFile(metaPath, existingId, &metaSource, &metaType)) {
      existingId.clear();
    }
  }

  if (existingId.empty()) {
    existingId = Aetherion::Core::GenerateUUID();
    writeMeta = true;
  }

  if (metaSource.empty() || metaSource != sourceLabel || metaType.empty() ||
      metaType != AssetRegistry::AssetTypeToString(type)) {
    writeMeta = true;
  }

  if (writeMeta) {
    WriteMetadataFile(metaPath, existingId, type, sourceLabel);
  }

  outId = existingId;
  return true;
}

std::filesystem::file_time_type
SafeWriteTime(const std::filesystem::path &path) {
  std::error_code ec;
  auto time = std::filesystem::last_write_time(path, ec);
  if (ec) {
    return {};
  }
  return time;
}

std::string MakePathKey(const std::filesystem::path &path,
                        const std::filesystem::path &root) {
  std::error_code ec;
  std::filesystem::path normalized =
      std::filesystem::weakly_canonical(path, ec);
  if (ec) {
    normalized = path.lexically_normal();
    ec.clear();
  }

  if (!root.empty()) {
    auto rel = std::filesystem::relative(normalized, root, ec);
    if (!ec && !rel.empty()) {
      normalized = rel;
    }
  }
  return normalized.generic_string();
}

AssetRegistry::AssetType ClassifyAssetType(const std::filesystem::path &path) {
  const std::string ext = ToLower(path.extension().string());
  if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" ||
      ext == ".bmp" || ext == ".gif" || ext == ".dds" || ext == ".ktx" ||
      ext == ".ktx2") {
    return AssetRegistry::AssetType::Texture;
  }
  if (ext == ".gltf" || ext == ".glb" || ext == ".obj" || ext == ".fbx" ||
      ext == ".dae") {
    return AssetRegistry::AssetType::Mesh;
  }
  if (ext == ".wav" || ext == ".mp3" || ext == ".ogg" || ext == ".flac" ||
      ext == ".aiff") {
    return AssetRegistry::AssetType::Audio;
  }
  if (ext == ".lua" || ext == ".py" || ext == ".js" || ext == ".cs") {
    return AssetRegistry::AssetType::Script;
  }
  if (ext == ".vert" || ext == ".frag" || ext == ".glsl" || ext == ".spv") {
    return AssetRegistry::AssetType::Shader;
  }
  if (ext == ".json") {
    return PathHasSegment(path, "scenes") ? AssetRegistry::AssetType::Scene
                                          : AssetRegistry::AssetType::Other;
  }
  return AssetRegistry::AssetType::Other;
}

void TransformPosition(const cgltf_float *matrix, const float in[3],
                       float out[3]) {
  out[0] = static_cast<float>(matrix[0] * in[0] + matrix[4] * in[1] +
                              matrix[8] * in[2] + matrix[12]);
  out[1] = static_cast<float>(matrix[1] * in[0] + matrix[5] * in[1] +
                              matrix[9] * in[2] + matrix[13]);
  out[2] = static_cast<float>(matrix[2] * in[0] + matrix[6] * in[1] +
                              matrix[10] * in[2] + matrix[14]);
}

void TransformDirection(const cgltf_float *matrix, const float in[3],
                        float out[3]) {
  out[0] = static_cast<float>(matrix[0] * in[0] + matrix[4] * in[1] +
                              matrix[8] * in[2]);
  out[1] = static_cast<float>(matrix[1] * in[0] + matrix[5] * in[1] +
                              matrix[9] * in[2]);
  out[2] = static_cast<float>(matrix[2] * in[0] + matrix[6] * in[1] +
                              matrix[10] * in[2]);
}

bool ComputeNormalMatrix(const cgltf_float *matrix, float out[9]) {
  const float a00 = static_cast<float>(matrix[0]);
  const float a01 = static_cast<float>(matrix[4]);
  const float a02 = static_cast<float>(matrix[8]);
  const float a10 = static_cast<float>(matrix[1]);
  const float a11 = static_cast<float>(matrix[5]);
  const float a12 = static_cast<float>(matrix[9]);
  const float a20 = static_cast<float>(matrix[2]);
  const float a21 = static_cast<float>(matrix[6]);
  const float a22 = static_cast<float>(matrix[10]);

  const float det = a00 * (a11 * a22 - a12 * a21) -
                    a01 * (a10 * a22 - a12 * a20) +
                    a02 * (a10 * a21 - a11 * a20);
  if (std::abs(det) < 1e-8f) {
    return false;
  }

  const float invDet = 1.0f / det;
  const float i00 = (a11 * a22 - a12 * a21) * invDet;
  const float i01 = (a02 * a21 - a01 * a22) * invDet;
  const float i02 = (a01 * a12 - a02 * a11) * invDet;
  const float i10 = (a12 * a20 - a10 * a22) * invDet;
  const float i11 = (a00 * a22 - a02 * a20) * invDet;
  const float i12 = (a02 * a10 - a00 * a12) * invDet;
  const float i20 = (a10 * a21 - a11 * a20) * invDet;
  const float i21 = (a01 * a20 - a00 * a21) * invDet;
  const float i22 = (a00 * a11 - a01 * a10) * invDet;

  out[0] = i00;
  out[1] = i10;
  out[2] = i20;
  out[3] = i01;
  out[4] = i11;
  out[5] = i21;
  out[6] = i02;
  out[7] = i12;
  out[8] = i22;
  return true;
}

void TransformNormal(const float normalMatrix[9], const float in[3],
                     float out[3]) {
  out[0] = normalMatrix[0] * in[0] + normalMatrix[3] * in[1] +
           normalMatrix[6] * in[2];
  out[1] = normalMatrix[1] * in[0] + normalMatrix[4] * in[1] +
           normalMatrix[7] * in[2];
  out[2] = normalMatrix[2] * in[0] + normalMatrix[5] * in[1] +
           normalMatrix[8] * in[2];
}

void NormalizeVector(float v[3], const float fallback[3]) {
  const float lenSq = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
  if (lenSq > 0.0f) {
    const float invLen = 1.0f / std::sqrt(lenSq);
    v[0] *= invLen;
    v[1] *= invLen;
    v[2] *= invLen;
  } else {
    v[0] = fallback[0];
    v[1] = fallback[1];
    v[2] = fallback[2];
  }
}

void ComputeMeshBounds(AssetRegistry::MeshData &mesh) {
  if (mesh.positions.empty()) {
    mesh.boundsMin = {0.0f, 0.0f, 0.0f};
    mesh.boundsMax = {0.0f, 0.0f, 0.0f};
    mesh.boundsCenter = {0.0f, 0.0f, 0.0f};
    mesh.boundsRadius = 0.0f;
    return;
  }

  std::array<float, 3> minV = mesh.positions.front();
  std::array<float, 3> maxV = mesh.positions.front();
  for (const auto &pos : mesh.positions) {
    minV[0] = std::min(minV[0], pos[0]);
    minV[1] = std::min(minV[1], pos[1]);
    minV[2] = std::min(minV[2], pos[2]);
    maxV[0] = std::max(maxV[0], pos[0]);
    maxV[1] = std::max(maxV[1], pos[1]);
    maxV[2] = std::max(maxV[2], pos[2]);
  }

  mesh.boundsMin = minV;
  mesh.boundsMax = maxV;
  mesh.boundsCenter = {(minV[0] + maxV[0]) * 0.5f, (minV[1] + maxV[1]) * 0.5f,
                       (minV[2] + maxV[2]) * 0.5f};

  float radiusSq = 0.0f;
  for (const auto &pos : mesh.positions) {
    const float dx = pos[0] - mesh.boundsCenter[0];
    const float dy = pos[1] - mesh.boundsCenter[1];
    const float dz = pos[2] - mesh.boundsCenter[2];
    radiusSq = std::max(radiusSq, dx * dx + dy * dy + dz * dz);
  }
  mesh.boundsRadius = std::sqrt(radiusSq);
}

void ComputeMeshNormals(AssetRegistry::MeshData &mesh) {
  if (mesh.positions.empty() || mesh.indices.size() < 3) {
    mesh.normals.assign(mesh.positions.size(), {0.0f, 0.0f, 1.0f});
    return;
  }

  mesh.normals.assign(mesh.positions.size(), {0.0f, 0.0f, 0.0f});
  for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
    const auto i0 = mesh.indices[i];
    const auto i1 = mesh.indices[i + 1];
    const auto i2 = mesh.indices[i + 2];
    if (i0 >= mesh.positions.size() || i1 >= mesh.positions.size() ||
        i2 >= mesh.positions.size()) {
      continue;
    }

    const auto &p0 = mesh.positions[i0];
    const auto &p1 = mesh.positions[i1];
    const auto &p2 = mesh.positions[i2];

    const float ux = p1[0] - p0[0];
    const float uy = p1[1] - p0[1];
    const float uz = p1[2] - p0[2];

    const float vx = p2[0] - p0[0];
    const float vy = p2[1] - p0[1];
    const float vz = p2[2] - p0[2];

    const float nx = uy * vz - uz * vy;
    const float ny = uz * vx - ux * vz;
    const float nz = ux * vy - uy * vx;

    mesh.normals[i0][0] += nx;
    mesh.normals[i0][1] += ny;
    mesh.normals[i0][2] += nz;
    mesh.normals[i1][0] += nx;
    mesh.normals[i1][1] += ny;
    mesh.normals[i1][2] += nz;
    mesh.normals[i2][0] += nx;
    mesh.normals[i2][1] += ny;
    mesh.normals[i2][2] += nz;
  }

  for (auto &n : mesh.normals) {
    const float lenSq = n[0] * n[0] + n[1] * n[1] + n[2] * n[2];
    if (lenSq > 0.0f) {
      const float invLen = 1.0f / std::sqrt(lenSq);
      n[0] *= invLen;
      n[1] *= invLen;
      n[2] *= invLen;
    } else {
      n = {0.0f, 0.0f, 1.0f};
    }
  }
}

bool IsFiniteVec3(const std::array<float, 3> &value) {
  return std::isfinite(value[0]) && std::isfinite(value[1]) &&
         std::isfinite(value[2]);
}

bool IsFiniteVec4(const std::array<float, 4> &value) {
  return std::isfinite(value[0]) && std::isfinite(value[1]) &&
         std::isfinite(value[2]) && std::isfinite(value[3]);
}

void ComputeMeshTangents(AssetRegistry::MeshData &mesh) {
  if (mesh.positions.empty() || mesh.indices.size() < 3 ||
      mesh.uvs.size() < mesh.positions.size()) {
    mesh.tangents.resize(mesh.positions.size(), {1.0f, 0.0f, 0.0f, 1.0f});
    return;
  }

  const size_t vertexCount = mesh.positions.size();
  std::vector<std::array<float, 3>> tan1(vertexCount, {0.0f, 0.0f, 0.0f});
  std::vector<std::array<float, 3>> tan2(vertexCount, {0.0f, 0.0f, 0.0f});

  for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
    const auto i0 = mesh.indices[i];
    const auto i1 = mesh.indices[i + 1];
    const auto i2 = mesh.indices[i + 2];
    if (i0 >= vertexCount || i1 >= vertexCount || i2 >= vertexCount) {
      continue;
    }

    const auto &p0 = mesh.positions[i0];
    const auto &p1 = mesh.positions[i1];
    const auto &p2 = mesh.positions[i2];
    const auto &w0 = mesh.uvs[i0];
    const auto &w1 = mesh.uvs[i1];
    const auto &w2 = mesh.uvs[i2];

    const float x1 = p1[0] - p0[0];
    const float y1 = p1[1] - p0[1];
    const float z1 = p1[2] - p0[2];
    const float x2 = p2[0] - p0[0];
    const float y2 = p2[1] - p0[1];
    const float z2 = p2[2] - p0[2];

    const float s1 = w1[0] - w0[0];
    const float t1 = w1[1] - w0[1];
    const float s2 = w2[0] - w0[0];
    const float t2 = w2[1] - w0[1];

    const float denom = s1 * t2 - s2 * t1;
    if (std::abs(denom) < 1e-8f) {
      continue;
    }

    const float r = 1.0f / denom;
    const float sdirX = (t2 * x1 - t1 * x2) * r;
    const float sdirY = (t2 * y1 - t1 * y2) * r;
    const float sdirZ = (t2 * z1 - t1 * z2) * r;
    const float tdirX = (s1 * x2 - s2 * x1) * r;
    const float tdirY = (s1 * y2 - s2 * y1) * r;
    const float tdirZ = (s1 * z2 - s2 * z1) * r;

    tan1[i0][0] += sdirX;
    tan1[i0][1] += sdirY;
    tan1[i0][2] += sdirZ;
    tan1[i1][0] += sdirX;
    tan1[i1][1] += sdirY;
    tan1[i1][2] += sdirZ;
    tan1[i2][0] += sdirX;
    tan1[i2][1] += sdirY;
    tan1[i2][2] += sdirZ;

    tan2[i0][0] += tdirX;
    tan2[i0][1] += tdirY;
    tan2[i0][2] += tdirZ;
    tan2[i1][0] += tdirX;
    tan2[i1][1] += tdirY;
    tan2[i1][2] += tdirZ;
    tan2[i2][0] += tdirX;
    tan2[i2][1] += tdirY;
    tan2[i2][2] += tdirZ;
  }

  mesh.tangents.resize(vertexCount);
  for (size_t i = 0; i < vertexCount; ++i) {
    float normal[3] = {mesh.normals[i][0], mesh.normals[i][1],
                       mesh.normals[i][2]};
    const float defaultNormal[3] = {0.0f, 0.0f, 1.0f};
    NormalizeVector(normal, defaultNormal);

    float tangent[3] = {tan1[i][0], tan1[i][1], tan1[i][2]};
    const float dotNT = normal[0] * tangent[0] + normal[1] * tangent[1] +
                        normal[2] * tangent[2];
    tangent[0] -= normal[0] * dotNT;
    tangent[1] -= normal[1] * dotNT;
    tangent[2] -= normal[2] * dotNT;

    const float lenSq = tangent[0] * tangent[0] + tangent[1] * tangent[1] +
                        tangent[2] * tangent[2];
    if (!(lenSq > 1e-12f)) {
      float axis[3] = {1.0f, 0.0f, 0.0f};
      if (std::abs(normal[0]) > 0.9f) {
        axis[0] = 0.0f;
        axis[1] = 1.0f;
      }
      tangent[0] = normal[1] * axis[2] - normal[2] * axis[1];
      tangent[1] = normal[2] * axis[0] - normal[0] * axis[2];
      tangent[2] = normal[0] * axis[1] - normal[1] * axis[0];
      const float fallback[3] = {1.0f, 0.0f, 0.0f};
      NormalizeVector(tangent, fallback);
    } else {
      const float invLen = 1.0f / std::sqrt(lenSq);
      tangent[0] *= invLen;
      tangent[1] *= invLen;
      tangent[2] *= invLen;
    }

    const float bitangentX = normal[1] * tangent[2] - normal[2] * tangent[1];
    const float bitangentY = normal[2] * tangent[0] - normal[0] * tangent[2];
    const float bitangentZ = normal[0] * tangent[1] - normal[1] * tangent[0];
    const auto &tanB = tan2[i];
    const float handedness = (bitangentX * tanB[0] + bitangentY * tanB[1] +
                              bitangentZ * tanB[2]) < 0.0f
                                 ? -1.0f
                                 : 1.0f;

    mesh.tangents[i] = {tangent[0], tangent[1], tangent[2], handedness};
  }
}

bool SanitizeMeshData(AssetRegistry::MeshData &mesh, bool recomputeNormals,
                      bool recomputeTangents) {
  if (mesh.positions.empty() || mesh.indices.size() < 3) {
    return false;
  }

  std::vector<bool> validVertices(mesh.positions.size(), true);
  for (size_t i = 0; i < mesh.positions.size(); ++i) {
    if (!IsFiniteVec3(mesh.positions[i])) {
      mesh.positions[i] = {0.0f, 0.0f, 0.0f};
      validVertices[i] = false;
    }
  }

  std::vector<std::uint32_t> sanitized;
  sanitized.reserve(mesh.indices.size());
  for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
    const auto i0 = mesh.indices[i];
    const auto i1 = mesh.indices[i + 1];
    const auto i2 = mesh.indices[i + 2];
    if (i0 >= mesh.positions.size() || i1 >= mesh.positions.size() ||
        i2 >= mesh.positions.size()) {
      continue;
    }
    if (!validVertices[i0] || !validVertices[i1] || !validVertices[i2]) {
      continue;
    }
    if (i0 == i1 || i1 == i2 || i0 == i2) {
      continue;
    }

    const auto &p0 = mesh.positions[i0];
    const auto &p1 = mesh.positions[i1];
    const auto &p2 = mesh.positions[i2];

    const float ux = p1[0] - p0[0];
    const float uy = p1[1] - p0[1];
    const float uz = p1[2] - p0[2];
    const float vx = p2[0] - p0[0];
    const float vy = p2[1] - p0[1];
    const float vz = p2[2] - p0[2];

    const float nx = uy * vz - uz * vy;
    const float ny = uz * vx - ux * vz;
    const float nz = ux * vy - uy * vx;
    const float areaSq = nx * nx + ny * ny + nz * nz;
    if (!(areaSq > 1e-12f)) {
      continue;
    }

    sanitized.push_back(i0);
    sanitized.push_back(i1);
    sanitized.push_back(i2);
  }

  mesh.indices.swap(sanitized);
  if (mesh.indices.empty()) {
    return false;
  }

  if (mesh.colors.size() < mesh.positions.size()) {
    mesh.colors.resize(mesh.positions.size(), {1.0f, 1.0f, 1.0f, 1.0f});
  }
  if (mesh.uvs.size() < mesh.positions.size()) {
    mesh.uvs.resize(mesh.positions.size(), {0.0f, 0.0f});
  }
  if (mesh.tangents.size() < mesh.positions.size()) {
    mesh.tangents.resize(mesh.positions.size(), {1.0f, 0.0f, 0.0f, 1.0f});
    recomputeTangents = true;
  }
  if (mesh.normals.size() < mesh.positions.size()) {
    mesh.normals.resize(mesh.positions.size(), {0.0f, 0.0f, 1.0f});
    recomputeNormals = true;
  }

  if (recomputeNormals) {
    recomputeTangents = true;
  }

  if (!recomputeNormals) {
    for (const auto &n : mesh.normals) {
      if (!IsFiniteVec3(n)) {
        recomputeNormals = true;
        recomputeTangents = true;
        break;
      }
      const float lenSq = n[0] * n[0] + n[1] * n[1] + n[2] * n[2];
      if (!(lenSq > 1e-12f)) {
        recomputeNormals = true;
        recomputeTangents = true;
        break;
      }
    }
  }

  if (!recomputeTangents) {
    for (const auto &t : mesh.tangents) {
      if (!IsFiniteVec4(t)) {
        recomputeTangents = true;
        break;
      }
      const float lenSq = t[0] * t[0] + t[1] * t[1] + t[2] * t[2];
      if (!(lenSq > 1e-12f)) {
        recomputeTangents = true;
        break;
      }
    }
  }

  if (recomputeNormals) {
    ComputeMeshNormals(mesh);
  }
  if (recomputeTangents) {
    ComputeMeshTangents(mesh);
  }
  ComputeMeshBounds(mesh);
  return true;
}

bool LoadObjMesh(const std::filesystem::path &sourcePath,
                 AssetRegistry::MeshData &mesh) {
  std::ifstream input(sourcePath);
  if (!input.is_open()) {
    return false;
  }

  struct ObjVertexKey {
    int position = -1;
    int texcoord = -1;
    int normal = -1;
  };

  struct ObjVertexKeyHash {
    size_t operator()(const ObjVertexKey &key) const noexcept {
      size_t seed = std::hash<int>{}(key.position);
      seed ^= std::hash<int>{}(key.texcoord) + 0x9e3779b9 + (seed << 6) +
              (seed >> 2);
      seed ^=
          std::hash<int>{}(key.normal) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
      return seed;
    }
  };

  struct ObjVertexKeyEq {
    bool operator()(const ObjVertexKey &lhs,
                    const ObjVertexKey &rhs) const noexcept {
      return lhs.position == rhs.position && lhs.texcoord == rhs.texcoord &&
             lhs.normal == rhs.normal;
    }
  };

  auto resolveIndex = [](int value, size_t count) -> int {
    if (value == 0) {
      return -1;
    }
    int idx = value;
    if (idx < 0) {
      idx = static_cast<int>(count) + idx;
    } else {
      idx -= 1;
    }
    if (idx < 0 || idx >= static_cast<int>(count)) {
      return -1;
    }
    return idx;
  };

  auto parseIndex = [](const std::string &value, int &outIndex) {
    if (value.empty()) {
      outIndex = 0;
      return;
    }
    try {
      outIndex = std::stoi(value);
    } catch (const std::exception &) {
      outIndex = 0;
    }
  };

  std::vector<std::array<float, 3>> positions;
  std::vector<std::array<float, 4>> colors;
  std::vector<std::array<float, 3>> normals;
  std::vector<std::array<float, 2>> texcoords;

  std::vector<std::array<float, 3>> outPositions;
  std::vector<std::array<float, 4>> outColors;
  std::vector<std::array<float, 3>> outNormals;
  std::vector<std::array<float, 2>> outUvs;
  std::vector<std::uint32_t> indices;

  std::unordered_map<ObjVertexKey, std::uint32_t, ObjVertexKeyHash,
                     ObjVertexKeyEq>
      vertexLookup;
  vertexLookup.reserve(1024);

  bool hasNormals = false;
  bool missingNormals = false;

  std::string line;
  while (std::getline(input, line)) {
    std::istringstream stream(line);
    std::string keyword;
    if (!(stream >> keyword)) {
      continue;
    }
    if (!keyword.empty() && keyword.front() == '#') {
      continue;
    }

    if (keyword == "v") {
      float x = 0.0f;
      float y = 0.0f;
      float z = 0.0f;
      if (!(stream >> x >> y >> z)) {
        continue;
      }

      float r = 1.0f;
      float g = 1.0f;
      float b = 1.0f;
      float a = 1.0f;
      if (stream >> r >> g >> b) {
        if (!(stream >> a)) {
          a = 1.0f;
        }
        colors.push_back({r, g, b, a});
      } else {
        colors.push_back({1.0f, 1.0f, 1.0f, 1.0f});
      }
      positions.push_back({x, y, z});
    } else if (keyword == "vn") {
      float nx = 0.0f;
      float ny = 0.0f;
      float nz = 0.0f;
      if (stream >> nx >> ny >> nz) {
        normals.push_back({nx, ny, nz});
      }
    } else if (keyword == "vt") {
      float u = 0.0f;
      float v = 0.0f;
      if (stream >> u >> v) {
        texcoords.push_back({u, v});
      }
    } else if (keyword == "f") {
      std::vector<std::uint32_t> face;
      std::string token;
      while (stream >> token) {
        if (positions.empty()) {
          continue;
        }

        int positionValue = 0;
        int texValue = 0;
        int normalValue = 0;

        const size_t slash = token.find('/');
        if (slash == std::string::npos) {
          parseIndex(token, positionValue);
        } else {
          parseIndex(token.substr(0, slash), positionValue);
          const size_t slash2 = token.find('/', slash + 1);
          if (slash2 == std::string::npos) {
            parseIndex(token.substr(slash + 1), texValue);
          } else {
            parseIndex(token.substr(slash + 1, slash2 - slash - 1), texValue);
            size_t slash3 = token.find('/', slash2 + 1);
            if (slash3 != std::string::npos) {
              parseIndex(token.substr(slash2 + 1, slash3 - slash2 - 1),
                         normalValue);
            } else {
              parseIndex(token.substr(slash2 + 1), normalValue);
            }
          }
        }

        const int positionIndex = resolveIndex(positionValue, positions.size());
        if (positionIndex < 0) {
          continue;
        }
        const int texIndex = resolveIndex(texValue, texcoords.size());
        const int normalIndex = resolveIndex(normalValue, normals.size());

        const ObjVertexKey key{positionIndex, texIndex, normalIndex};
        auto it = vertexLookup.find(key);
        std::uint32_t vertexIndex = 0;
        if (it == vertexLookup.end()) {
          outPositions.push_back(positions[static_cast<size_t>(positionIndex)]);
          if (static_cast<size_t>(positionIndex) < colors.size()) {
            outColors.push_back(colors[static_cast<size_t>(positionIndex)]);
          } else {
            outColors.push_back({1.0f, 1.0f, 1.0f, 1.0f});
          }

          if (texIndex >= 0 &&
              static_cast<size_t>(texIndex) < texcoords.size()) {
            outUvs.push_back(texcoords[static_cast<size_t>(texIndex)]);
          } else {
            outUvs.push_back({0.0f, 0.0f});
          }

          if (normalIndex >= 0 &&
              static_cast<size_t>(normalIndex) < normals.size()) {
            auto normal = normals[static_cast<size_t>(normalIndex)];
            float normalValues[3] = {normal[0], normal[1], normal[2]};
            const float defaultNormal[3] = {0.0f, 0.0f, 1.0f};
            NormalizeVector(normalValues, defaultNormal);
            outNormals.push_back(
                {normalValues[0], normalValues[1], normalValues[2]});
            hasNormals = true;
          } else {
            outNormals.push_back({0.0f, 0.0f, 1.0f});
            missingNormals = true;
          }

          vertexIndex = static_cast<std::uint32_t>(outPositions.size() - 1);
          vertexLookup.emplace(key, vertexIndex);
        } else {
          vertexIndex = it->second;
        }
        face.push_back(vertexIndex);
      }

      if (face.size() >= 3) {
        for (size_t i = 1; i + 1 < face.size(); ++i) {
          indices.push_back(face[0]);
          indices.push_back(face[i]);
          indices.push_back(face[i + 1]);
        }
      }
    }
  }

  if (outPositions.empty() || indices.empty()) {
    return false;
  }

  mesh.positions = std::move(outPositions);
  mesh.colors = std::move(outColors);
  mesh.normals = std::move(outNormals);
  mesh.uvs = std::move(outUvs);
  mesh.indices = std::move(indices);

  return SanitizeMeshData(mesh, !hasNormals || missingNormals, true);
}

int AssetTypeOrder(AssetRegistry::AssetType type) {
  switch (type) {
  case AssetRegistry::AssetType::Texture:
    return 0;
  case AssetRegistry::AssetType::Mesh:
    return 1;
  case AssetRegistry::AssetType::Audio:
    return 2;
  case AssetRegistry::AssetType::Script:
    return 3;
  case AssetRegistry::AssetType::Scene:
    return 4;
  case AssetRegistry::AssetType::Shader:
    return 5;
  case AssetRegistry::AssetType::Other:
  default:
    return 6;
  }
}
} // namespace

namespace Aetherion::Assets {
const char *AssetRegistry::AssetTypeToString(AssetType type) {
  switch (type) {
  case AssetType::Texture:
    return "Texture";
  case AssetType::Mesh:
    return "Mesh";
  case AssetType::Audio:
    return "Audio";
  case AssetType::Script:
    return "Script";
  case AssetType::Scene:
    return "Scene";
  case AssetType::Shader:
    return "Shader";
  case AssetType::Other:
  default:
    return "Other";
  }
}

void AssetRegistry::Scan(const std::string &rootPath) {
  std::unordered_map<std::string, FileState> previousStates = m_fileStates;
  std::unordered_map<std::string, AssetType> previousTypes;
  for (const auto &entry : m_entries) {
    previousTypes.emplace(entry.id, entry.type);
  }

  m_placeholderAssets.clear();
  m_entries.clear();
  m_entryLookup.clear();
  m_pathToId.clear();

  std::error_code ec;
  std::filesystem::path nextRoot = std::filesystem::absolute(rootPath, ec);
  if (ec) {
    ec.clear();
    nextRoot = std::filesystem::path(rootPath);
  }
  const bool rootChanged = (!m_rootPath.empty() && nextRoot != m_rootPath);
  m_rootPath = nextRoot;
  m_placeholderAssets.emplace("root", m_rootPath.string());

  if (rootChanged) {
    previousStates.clear();
    m_meshData.clear();
    m_meshes.clear();
    m_textures.clear();
    m_materials.clear();
    m_changeLog.clear();
    m_changeSerial = 0;
  }

  std::unordered_map<std::string, FileState> nextStates;
  std::unordered_map<std::string, AssetType> nextTypes;

  if (std::filesystem::exists(m_rootPath, ec)) {
    const auto options =
        std::filesystem::directory_options::skip_permission_denied;
    for (auto it = std::filesystem::recursive_directory_iterator(m_rootPath,
                                                                 options, ec);
         it != std::filesystem::recursive_directory_iterator();
         it.increment(ec)) {
      if (ec) {
        ec.clear();
        continue;
      }

      const auto &entry = *it;
      if (!entry.is_regular_file(ec)) {
        continue;
      }

      if (IsMetadataPath(entry.path())) {
        continue;
      }

      const auto filename = entry.path().filename().string();
      if (!filename.empty() && filename.front() == '.') {
        continue;
      }

      std::filesystem::path relative =
          std::filesystem::relative(entry.path(), m_rootPath, ec);
      std::string sourceLabel = (!ec && !relative.empty())
                                    ? relative.generic_string()
                                    : entry.path().filename().generic_string();
      if (sourceLabel.empty()) {
        continue;
      }

      const std::filesystem::path metaPath = BuildMetadataPath(entry.path());
      std::string assetId;
      std::string metaSource;
      std::string metaType;
      bool writeMeta = false;

      if (std::filesystem::exists(metaPath, ec)) {
        if (!ReadMetadataFile(metaPath, assetId, &metaSource, &metaType)) {
          assetId.clear();
        }
      }

      if (assetId.empty()) {
        assetId = Core::GenerateUUID();
        writeMeta = true;
      }

      const AssetType type = ClassifyAssetType(entry.path());
      if (metaSource.empty() || metaSource != sourceLabel) {
        writeMeta = true;
      }
      if (metaType.empty() || metaType != AssetTypeToString(type)) {
        writeMeta = true;
      }

      if (writeMeta) {
        WriteMetadataFile(metaPath, assetId, type, sourceLabel);
      }

      AssetEntry asset{};
      asset.id = assetId;
      asset.path = entry.path();
      asset.type = type;

      m_entryLookup.emplace(asset.id, m_entries.size());
      m_entries.push_back(std::move(asset));
      m_pathToId.emplace(MakePathKey(entry.path(), m_rootPath), assetId);

      FileState state{};
      state.path = entry.path();
      state.assetTime = SafeWriteTime(entry.path());
      state.metaTime = SafeWriteTime(metaPath);
      nextStates.emplace(assetId, state);
      nextTypes.emplace(assetId, type);
    }
  }

  std::sort(m_entries.begin(), m_entries.end(),
            [](const AssetEntry &a, const AssetEntry &b) {
              const int orderA = AssetTypeOrder(a.type);
              const int orderB = AssetTypeOrder(b.type);
              if (orderA != orderB) {
                return orderA < orderB;
              }
              return a.id < b.id;
            });

  m_entryLookup.clear();
  for (size_t i = 0; i < m_entries.size(); ++i) {
    m_entryLookup.emplace(m_entries[i].id, i);
  }

  std::vector<AssetChange> scanChanges;
  auto recordChange = [this, &scanChanges](const std::string &id,
                                           AssetType type,
                                           AssetChange::Kind kind) {
    AssetChange change{};
    change.id = id;
    change.type = type;
    change.kind = kind;
    change.serial = ++m_changeSerial;
    m_changeLog.push_back(change);
    scanChanges.push_back(change);
  };

  for (const auto &[id, state] : nextStates) {
    auto prevIt = previousStates.find(id);
    if (prevIt == previousStates.end()) {
      recordChange(id, nextTypes[id], AssetChange::Kind::Added);
      continue;
    }

    const FileState &prev = prevIt->second;
    if (prev.path != state.path) {
      recordChange(id, nextTypes[id], AssetChange::Kind::Moved);
      continue;
    }

    if (prev.assetTime != state.assetTime || prev.metaTime != state.metaTime) {
      const bool onlyMeta = (prev.assetTime == state.assetTime) &&
                            (prev.metaTime != state.metaTime);
      recordChange(id, nextTypes[id],
                   onlyMeta ? AssetChange::Kind::Metadata
                            : AssetChange::Kind::Modified);
    }
  }

  for (const auto &[id, prev] : previousStates) {
    if (nextStates.find(id) != nextStates.end()) {
      continue;
    }
    AssetType type = AssetType::Other;
    if (auto typeIt = previousTypes.find(id); typeIt != previousTypes.end()) {
      type = typeIt->second;
    } else {
      type = ClassifyAssetType(prev.path);
    }
    recordChange(id, type, AssetChange::Kind::Removed);
  }

  for (const auto &change : scanChanges) {
    if (change.kind == AssetChange::Kind::Removed ||
        change.kind == AssetChange::Kind::Modified ||
        change.kind == AssetChange::Kind::Moved) {
      m_meshData.erase(change.id);
      m_meshes.erase(change.id);
      m_textures.erase(change.id);

      if (change.type == AssetType::Mesh) {
        for (auto it = m_materials.begin(); it != m_materials.end();) {
          if (it->first.rfind(change.id + ":", 0) == 0) {
            it = m_materials.erase(it);
          } else {
            ++it;
          }
        }
      }
    }
  }

  const size_t maxChanges = 2048;
  if (m_changeLog.size() > maxChanges) {
    m_changeLog.erase(
        m_changeLog.begin(),
        m_changeLog.begin() +
            static_cast<std::ptrdiff_t>(m_changeLog.size() - maxChanges));
  }

  m_fileStates = std::move(nextStates);
}

void AssetRegistry::Rescan() {
  if (m_rootPath.empty()) {
    Scan("assets");
    return;
  }

  Scan(m_rootPath.string());
}

bool AssetRegistry::HasAsset(const std::string &assetId) const {
  return m_placeholderAssets.find(assetId) != m_placeholderAssets.end() ||
         m_entryLookup.find(assetId) != m_entryLookup.end() ||
         m_meshes.find(assetId) != m_meshes.end() ||
         m_textures.find(assetId) != m_textures.end() ||
         m_materials.find(assetId) != m_materials.end() ||
         m_meshData.find(assetId) != m_meshData.end();
}

const std::vector<AssetRegistry::AssetEntry> &
AssetRegistry::GetEntries() const noexcept {
  return m_entries;
}

const std::filesystem::path &AssetRegistry::GetRootPath() const noexcept {
  return m_rootPath;
}

const AssetRegistry::AssetEntry *
AssetRegistry::FindEntry(const std::string &assetId) const noexcept {
  auto it = m_entryLookup.find(assetId);
  if (it != m_entryLookup.end()) {
    const size_t index = it->second;
    if (index < m_entries.size()) {
      return &m_entries[index];
    }
  }

  if (!assetId.empty() && !m_rootPath.empty()) {
    std::filesystem::path assetPath(assetId);
    if (!assetPath.is_absolute()) {
      assetPath = m_rootPath / assetPath;
    }
    const std::string key = MakePathKey(assetPath, m_rootPath);
    auto pathIt = m_pathToId.find(key);
    if (pathIt != m_pathToId.end()) {
      auto entryIt = m_entryLookup.find(pathIt->second);
      if (entryIt != m_entryLookup.end()) {
        const size_t index = entryIt->second;
        if (index < m_entries.size()) {
          return &m_entries[index];
        }
      }
    }
  }

  return nullptr;
}

const AssetRegistry::MeshData *
AssetRegistry::GetMeshData(const std::string &assetId) const noexcept {
  auto it = m_meshData.find(assetId);
  if (it == m_meshData.end()) {
    return nullptr;
  }
  return &it->second;
}

const AssetRegistry::MeshData *
AssetRegistry::LoadMeshData(const std::string &assetId) {
  if (assetId.empty()) {
    return nullptr;
  }

  if (const auto *cached = GetMeshData(assetId)) {
    return cached;
  }

  std::filesystem::path sourcePath;
  if (const auto *entry = FindEntry(assetId)) {
    sourcePath = entry->path;
  } else {
    sourcePath = std::filesystem::path(assetId);
    if (!sourcePath.is_absolute() && !m_rootPath.empty()) {
      sourcePath = m_rootPath / sourcePath;
    }
  }

  std::error_code ec;
  if (sourcePath.empty() || !std::filesystem::exists(sourcePath, ec)) {
    return nullptr;
  }

  const std::string extension = ToLower(sourcePath.extension().string());
  if (extension == ".obj") {
    MeshData mesh{};
    if (!LoadObjMesh(sourcePath, mesh)) {
      return nullptr;
    }

    auto &stored = m_meshData[assetId];
    stored = std::move(mesh);
    return &stored;
  }

  if (extension != ".gltf" && extension != ".glb") {
    return nullptr;
  }

  cgltf_options options{};
  cgltf_data *data = nullptr;
  cgltf_result result =
      cgltf_parse_file(&options, sourcePath.string().c_str(), &data);
  if (result != cgltf_result_success || !data) {
    if (data) {
      cgltf_free(data);
    }
    return nullptr;
  }

  result = cgltf_load_buffers(&options, data, sourcePath.string().c_str());
  if (result != cgltf_result_success) {
    cgltf_free(data);
    return nullptr;
  }

  MeshData mesh{};
  bool loadedNormals = false;
  bool loadedTangents = false;

  auto appendPrimitive = [&](const cgltf_primitive &primitive,
                             const cgltf_float *matrix) {
    if (primitive.type != cgltf_primitive_type_triangles) {
      return;
    }

    const cgltf_accessor *positionAccessor = nullptr;
    const cgltf_accessor *colorAccessor = nullptr;
    const cgltf_accessor *normalAccessor = nullptr;
    const cgltf_accessor *uvAccessor = nullptr;
    const cgltf_accessor *tangentAccessor = nullptr;
    for (cgltf_size attrIndex = 0; attrIndex < primitive.attributes_count;
         ++attrIndex) {
      const cgltf_attribute &attr = primitive.attributes[attrIndex];
      if (attr.type == cgltf_attribute_type_position) {
        positionAccessor = attr.data;
      } else if (attr.type == cgltf_attribute_type_color) {
        colorAccessor = attr.data;
      } else if (attr.type == cgltf_attribute_type_normal) {
        normalAccessor = attr.data;
      } else if (attr.type == cgltf_attribute_type_tangent) {
        tangentAccessor = attr.data;
      } else if (attr.type == cgltf_attribute_type_texcoord &&
                 attr.index == 0) {
        uvAccessor = attr.data;
      }
    }

    if (!positionAccessor || positionAccessor->count == 0) {
      return;
    }

    const size_t baseVertex = mesh.positions.size();
    mesh.positions.reserve(baseVertex + positionAccessor->count);
    mesh.colors.reserve(baseVertex + positionAccessor->count);
    mesh.normals.reserve(baseVertex + positionAccessor->count);
    mesh.uvs.reserve(baseVertex + positionAccessor->count);

    float normalMatrix[9] = {};
    const bool hasNormalMatrix =
        matrix ? ComputeNormalMatrix(matrix, normalMatrix) : false;
    for (cgltf_size i = 0; i < positionAccessor->count; ++i) {
      float values[3] = {0.0f, 0.0f, 0.0f};
      cgltf_accessor_read_float(positionAccessor, i, values, 3);
      if (matrix) {
        float transformed[3];
        TransformPosition(matrix, values, transformed);
        mesh.positions.push_back(
            {transformed[0], transformed[1], transformed[2]});
      } else {
        mesh.positions.push_back({values[0], values[1], values[2]});
      }

      // Load vertex color if available, otherwise use white
      if (colorAccessor && i < colorAccessor->count) {
        float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        cgltf_accessor_read_float(colorAccessor, i, color, 4);
        mesh.colors.push_back({color[0], color[1], color[2], color[3]});
      } else {
        mesh.colors.push_back({1.0f, 1.0f, 1.0f, 1.0f});
      }

      float normal[3] = {0.0f, 0.0f, 1.0f};
      if (normalAccessor && i < normalAccessor->count) {
        cgltf_accessor_read_float(normalAccessor, i, normal, 3);
        loadedNormals = true;
      }
      if (hasNormalMatrix) {
        float transformed[3];
        TransformNormal(normalMatrix, normal, transformed);
        normal[0] = transformed[0];
        normal[1] = transformed[1];
        normal[2] = transformed[2];
      }
      const float defaultNormal[3] = {0.0f, 0.0f, 1.0f};
      NormalizeVector(normal, defaultNormal);
      mesh.normals.push_back({normal[0], normal[1], normal[2]});

      float uv[2] = {0.0f, 0.0f};
      if (uvAccessor && i < uvAccessor->count) {
        cgltf_accessor_read_float(uvAccessor, i, uv, 2);
      }
      mesh.uvs.push_back({uv[0], uv[1]});

      float tangent[4] = {1.0f, 0.0f, 0.0f, 1.0f};
      if (tangentAccessor && i < tangentAccessor->count) {
        cgltf_accessor_read_float(tangentAccessor, i, tangent, 4);
        loadedTangents = true;
      }
      if (hasNormalMatrix) {
        float transformed[3];
        TransformNormal(normalMatrix, tangent, transformed);
        tangent[0] = transformed[0];
        tangent[1] = transformed[1];
        tangent[2] = transformed[2];
      }
      const float defaultTangent[3] = {1.0f, 0.0f, 0.0f};
      NormalizeVector(tangent, defaultTangent);
      mesh.tangents.push_back({tangent[0], tangent[1], tangent[2], tangent[3]});
    }

    if (primitive.indices) {
      mesh.indices.reserve(mesh.indices.size() + primitive.indices->count);
      for (cgltf_size i = 0; i < primitive.indices->count; ++i) {
        const cgltf_size idx = cgltf_accessor_read_index(primitive.indices, i);
        mesh.indices.push_back(static_cast<std::uint32_t>(baseVertex + idx));
      }
    } else {
      mesh.indices.reserve(mesh.indices.size() + positionAccessor->count);
      for (cgltf_size i = 0; i < positionAccessor->count; ++i) {
        mesh.indices.push_back(static_cast<std::uint32_t>(baseVertex + i));
      }
    }
  };

  auto appendMesh = [&](const cgltf_mesh *srcMesh, const cgltf_float *matrix) {
    if (!srcMesh) {
      return;
    }
    for (cgltf_size primIndex = 0; primIndex < srcMesh->primitives_count;
         ++primIndex) {
      appendPrimitive(srcMesh->primitives[primIndex], matrix);
    }
  };

  auto traverseNode = [&](const cgltf_node *node, auto &&self) -> void {
    if (!node) {
      return;
    }

    cgltf_float matrix[16];
    cgltf_node_transform_world(node, matrix);
    appendMesh(node->mesh, matrix);

    for (cgltf_size i = 0; i < node->children_count; ++i) {
      self(node->children[i], self);
    }
  };

  bool usedNodes = false;
  if (data->scene && data->scene->nodes_count > 0) {
    usedNodes = true;
    for (cgltf_size i = 0; i < data->scene->nodes_count; ++i) {
      traverseNode(data->scene->nodes[i], traverseNode);
    }
  } else if (data->nodes_count > 0) {
    usedNodes = true;
    for (cgltf_size i = 0; i < data->nodes_count; ++i) {
      traverseNode(&data->nodes[i], traverseNode);
    }
  }

  if (!usedNodes) {
    for (cgltf_size meshIndex = 0; meshIndex < data->meshes_count;
         ++meshIndex) {
      appendMesh(&data->meshes[meshIndex], nullptr);
    }
  }

  cgltf_free(data);

  if (!SanitizeMeshData(mesh, !loadedNormals, !loadedTangents)) {
    return nullptr;
  }

  auto &stored = m_meshData[assetId];
  stored = std::move(mesh);
  return &stored;
}

AssetRegistry::GltfImportResult
AssetRegistry::ImportGltf(const std::string &gltfPath, bool forceReimport) {
  GltfImportResult result{};

  std::filesystem::path source(gltfPath);
  if (!std::filesystem::exists(source)) {
    result.message = "GLTF file not found";
    return result;
  }

  source = std::filesystem::absolute(source);
  const std::filesystem::path root =
      m_rootPath.empty() ? source.parent_path() : m_rootPath;

  std::string meshId;
  EnsureMetadataForAsset(source, root, AssetType::Mesh, meshId);

  if (!forceReimport) {
    if (auto cached = m_meshes.find(meshId); cached != m_meshes.end()) {
      result.success = true;
      result.id = meshId;
      result.textures = cached->second.textureIds;
      result.materials = cached->second.materialIds;
      result.message = "Cached GLTF";
      return result;
    }
  }

  cgltf_options options{};
  cgltf_data *data = nullptr;
  cgltf_result parseResult =
      cgltf_parse_file(&options, source.string().c_str(), &data);
  if (parseResult != cgltf_result_success || !data) {
    if (data) {
      cgltf_free(data);
    }
    result.message = "Unable to parse GLTF";
    return result;
  }

  parseResult = cgltf_load_buffers(&options, data, source.string().c_str());
  if (parseResult != cgltf_result_success) {
    cgltf_free(data);
    result.message = "Unable to load GLTF buffers";
    return result;
  }

  CachedMesh mesh{};
  mesh.id = meshId;
  mesh.source = source;
  std::unordered_set<std::string> uniqueTextures;

  std::vector<std::string> imageIds;
  imageIds.reserve(data->images_count);
  for (cgltf_size i = 0; i < data->images_count; ++i) {
    const cgltf_image &image = data->images[i];
    std::string texId;

    if (image.uri && image.uri[0] != '\0') {
      std::filesystem::path texPath = source.parent_path() / image.uri;
      if (std::filesystem::exists(texPath)) {
        EnsureMetadataForAsset(texPath, root, AssetType::Texture, texId);
        if (!texId.empty()) {
          CachedTexture tex{};
          tex.id = texId;
          tex.path = texPath;
          m_textures[texId] = tex;
          if (uniqueTextures.emplace(texId).second) {
            mesh.textureIds.push_back(texId);
          }
        }
      }
    }
    imageIds.push_back(texId);
  }

  std::vector<std::string> textureToImageId;
  textureToImageId.reserve(data->textures_count);
  for (cgltf_size i = 0; i < data->textures_count; ++i) {
    const cgltf_texture &tex = data->textures[i];
    std::string imageId;
    if (tex.image) {
      const cgltf_size imageIndex =
          static_cast<cgltf_size>(tex.image - data->images);
      if (imageIndex < imageIds.size()) {
        imageId = imageIds[imageIndex];
      }
    }
    textureToImageId.push_back(imageId);
  }

  for (cgltf_size i = 0; i < data->materials_count; ++i) {
    const cgltf_material &material = data->materials[i];
    std::string matId = meshId + ":mat:" + std::to_string(i);

    CachedMaterial cached{};
    cached.id = matId;
    cached.name = material.name ? material.name : std::string();

    if (material.has_pbr_metallic_roughness) {
      const auto &pbr = material.pbr_metallic_roughness;
      cached.baseColor = {pbr.base_color_factor[0], pbr.base_color_factor[1],
                          pbr.base_color_factor[2], pbr.base_color_factor[3]};
      cached.metallic = pbr.metallic_factor;
      cached.roughness = pbr.roughness_factor;
      if (pbr.base_color_texture.texture) {
        const cgltf_size texIndex = static_cast<cgltf_size>(
            pbr.base_color_texture.texture - data->textures);
        if (texIndex < textureToImageId.size()) {
          cached.albedoTextureId = textureToImageId[texIndex];
        }
      }
    }

    m_materials[matId] = cached;
    mesh.materialIds.push_back(matId);
    result.materials.push_back(matId);
    if (!cached.albedoTextureId.empty()) {
      if (uniqueTextures.emplace(cached.albedoTextureId).second) {
        mesh.textureIds.push_back(cached.albedoTextureId);
      }
    }
  }

  cgltf_free(data);

  m_meshes[meshId] = mesh;

  result.success = true;
  result.id = meshId;
  result.textures = mesh.textureIds;
  result.message = "Imported GLTF";
  return result;
}

const AssetRegistry::CachedMesh *
AssetRegistry::GetMesh(const std::string &id) const {
  auto it = m_meshes.find(id);
  if (it == m_meshes.end()) {
    return nullptr;
  }
  return &it->second;
}

const AssetRegistry::CachedTexture *
AssetRegistry::GetTexture(const std::string &id) const {
  auto it = m_textures.find(id);
  if (it == m_textures.end()) {
    return nullptr;
  }
  return &it->second;
}

const AssetRegistry::CachedMaterial *
AssetRegistry::GetMaterial(const std::string &id) const {
  auto it = m_materials.find(id);
  if (it == m_materials.end()) {
    return nullptr;
  }
  return &it->second;
}

std::uint64_t AssetRegistry::GetChangeSerial() const noexcept {
  return m_changeSerial;
}

void AssetRegistry::GetChangesSince(std::uint64_t serial,
                                    std::vector<AssetChange> &out) const {
  out.clear();
  for (const auto &change : m_changeLog) {
    if (change.serial > serial) {
      out.push_back(change);
    }
  }
}

std::filesystem::path
AssetRegistry::GetMetadataPathForAsset(const std::filesystem::path &assetPath) {
  return BuildMetadataPath(assetPath);
}
} // namespace Aetherion::Assets
