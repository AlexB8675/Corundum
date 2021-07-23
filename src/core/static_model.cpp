#include <corundum/core/static_texture.hpp>
#include <corundum/core/static_model.hpp>
#include <corundum/core/static_mesh.hpp>
#include <corundum/core/context.hpp>

#include <corundum/util/file_view.hpp>

#include <assimp/postprocess.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>

#include <unordered_map>
#include <filesystem>
#include <algorithm>
#include <string>
#include <vector>

namespace crd::core {
    using TextureCache = std::unordered_map<std::string, Async<StaticTexture>*>;
    namespace fs = std::filesystem;

    crd_nodiscard static inline Async<StaticTexture>* import_texture(const Context& context,
                                                                     const aiMaterial* material,
                                                                     aiTextureType type,
                                                                     TextureCache& cache,
                                                                     const fs::path& path) noexcept {
        crd_unlikely_if(material->GetTextureCount(type) == 0) {
            return new Async<StaticTexture>(); // Return an empty "Async<T>" object.
        }
        aiString str;
        material->GetTexture(type, 0, &str);
        auto file_name = (path / str.C_Str()).string();
        const auto format = type == aiTextureType_DIFFUSE ? texture_srgb : texture_unorm;
        const auto [cached, miss] = cache.try_emplace(file_name);
        crd_likely_if(!miss) {
            return cached->second;
        }
        return cached->second = new Async<StaticTexture>(request_static_texture(context, std::move(file_name), format));
    }

    crd_nodiscard static inline TexturedMesh import_textured_mesh(const Context& context,
                                                                  const aiScene* scene,
                                                                  const aiMesh* mesh,
                                                                  TextureCache& cache,
                                                                  const fs::path& path) noexcept {
        constexpr auto components = 14;
        std::vector<float> geometry;
        std::vector<std::uint32_t> indices;

        geometry.resize(mesh->mNumVertices * components);
        auto* ptr = &geometry.front();
        for (std::size_t i = 0; i < mesh->mNumVertices; ++i) {
            std::memcpy(ptr, &mesh->mVertices[i], sizeof(aiVector3D));
            crd_likely_if(mesh->mNormals)       { std::memcpy(ptr + 3, &mesh->mNormals[i], sizeof(aiVector3D)); }
            crd_likely_if(mesh->mTextureCoords) { std::memcpy(ptr + 6, &mesh->mTextureCoords[0][i], sizeof(aiVector2D)); }
            crd_likely_if(mesh->mTangents)      { std::memcpy(ptr + 8, &mesh->mTangents[i], sizeof(aiVector3D)); }
            crd_likely_if(mesh->mBitangents)    { std::memcpy(ptr + 11, &mesh->mBitangents[i], sizeof(aiVector3D)); }
            ptr += components;
        }

        indices.reserve(mesh->mNumFaces * 3);
        for (std::size_t i = 0; i < mesh->mNumFaces; i++) {
            auto& face = mesh->mFaces[i];
            for (std::size_t j = 0; j < face.mNumIndices; j++) {
                indices.emplace_back(face.mIndices[j]);
            }
        }
        const auto material = scene->mMaterials[mesh->mMaterialIndex];
        const auto vertex_size = geometry.size() / components;
        const auto index_size = indices.size();
        return {
            .mesh = request_static_mesh(context, { std::move(geometry), std::move(indices) }),
            .diffuse = import_texture(context, material, aiTextureType_DIFFUSE, cache, path),
            .normal = import_texture(context, material, aiTextureType_HEIGHT, cache, path),
            .specular = import_texture(context, material, aiTextureType_SPECULAR, cache, path),
            .vertices = static_cast<std::uint32_t>(vertex_size),
            .indices = static_cast<std::uint32_t>(index_size)
        };
    }

    static inline void process_node(const Context& context,
                             const aiScene* scene,
                             const aiNode* node,
                             StaticModel& model,
                             TextureCache& cache,
                             const fs::path& path) noexcept {
        for (std::size_t i = 0; i < node->mNumMeshes; i++) {
            model.emplace_back(import_textured_mesh(context, scene, scene->mMeshes[node->mMeshes[i]], cache, path));
        }

        for (std::size_t i = 0; i < node->mNumChildren; i++) {
            process_node(context, scene, node->mChildren[i], model, cache, path);
        }
    }

    crd_nodiscard crd_module Async<StaticModel> request_static_model(const Context& context, const char* path) noexcept {
        auto* task = new std::packaged_task<StaticModel()>(
            [&context, path]() {
                Assimp::Importer importer;
                const auto post_process =
                    aiProcess_Triangulate |
                    aiProcess_FlipUVs     |
                    aiProcess_GenNormals  |
                    aiProcess_CalcTangentSpace;
                const auto scene = importer.ReadFile(path, post_process);
                crd_assert(scene && !scene->mFlags && scene->mRootNode, "failed to load model");
                TextureCache cache;
                StaticModel model;
                process_node(context, scene, scene->mRootNode, model, cache, fs::path(path).parent_path());
                return model;
            });
        Async<StaticModel> resource;
        resource.future = task->get_future();
        context.scheduler->AddTask({
            .Function = [](ftl::TaskScheduler*, void* data) {
                auto* task = static_cast<std::packaged_task<StaticModel()>*>(data);
                (*task)();
                delete task;
            },
            .ArgData = task
        }, ftl::TaskPriority::High);
        return resource;
    }

    crd_module void destroy_static_model(const Context& context, StaticModel& model) noexcept {
        std::vector<Async<StaticTexture>*> to_destroy;
        to_destroy.resize(model.size() * 3);
        for (std::size_t i = 0; auto& each : model) {
            destroy_static_mesh(context, *each.mesh);
            to_destroy[i]     = each.diffuse;
            to_destroy[i + 1] = each.normal;
            to_destroy[i + 2] = each.specular;
            i += 3;
        }
        std::sort(to_destroy.begin(), to_destroy.end());
        to_destroy.erase(std::unique(to_destroy.begin(), to_destroy.end()), to_destroy.end());
        for (const auto each : to_destroy) {
            crd_likely_if(each->valid()) {
                destroy_static_texture(context, **each);
            }
            delete each;
        }
        model.clear();
    }
} // namespace crd::core
