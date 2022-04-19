#include <corundum/core/static_texture.hpp>
#include <corundum/core/static_model.hpp>
#include <corundum/core/static_mesh.hpp>
#include <corundum/core/context.hpp>

#include <corundum/detail/file_view.hpp>

#include <assimp/DefaultIOStream.h>
#include <assimp/DefaultIOSystem.h>
#include <assimp/postprocess.h>
#include <assimp/IOSystem.hpp>
#include <assimp/IOStream.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>

#include <Tracy.hpp>

#include <spdlog/spdlog.h>

#include <glm/vec3.hpp>
#include <glm/vec2.hpp>

#include <unordered_set>
#include <unordered_map>
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <string>
#include <vector>

namespace crd {
    using TextureCache = std::unordered_map<std::string, Async<StaticTexture>*>;
    namespace fs = std::filesystem;

    struct FileViewStream : Assimp::IOStream {
        dtl::FileView handle;
        std::size_t offset;

        FileViewStream() noexcept = default;

        explicit FileViewStream(dtl::FileView handle) noexcept
            : handle(handle),
              offset() {
            crd_profile_scoped();
        }

        ~FileViewStream() noexcept override {
            crd_profile_scoped();
            dtl::destroy_file_view(handle);
        }

        crd_nodiscard std::size_t Read(void* buffer, std::size_t size, std::size_t count) noexcept override {
            crd_profile_scoped();
            crd_unlikely_if(offset >= handle.size) {
                return 0;
            }
            auto bytes = size * count;
            crd_unlikely_if(offset + bytes >= handle.size) {
                bytes = handle.size - offset;
            }
            std::memcpy(buffer, static_cast<const char*>(handle.data) + offset, bytes);
            offset += bytes;
            return count;
        }

        crd_nodiscard std::size_t Write(const void*, std::size_t, std::size_t) noexcept override {
            return 0;
        }

        crd_nodiscard aiReturn Seek(std::size_t where, aiOrigin origin) noexcept override {
            crd_profile_scoped();
            switch (origin) {
                case aiOrigin_SET: { offset = where; } break;
                case aiOrigin_CUR: { offset += where; } break;
                case aiOrigin_END: { offset -= where; } break;
            }
            return aiReturn_SUCCESS;
        }

        crd_nodiscard std::size_t Tell() const noexcept override {
            crd_profile_scoped();
            return offset;
        }

        crd_nodiscard std::size_t FileSize() const noexcept override {
            crd_profile_scoped();
            return handle.size;
        }

        void Flush() noexcept override {}
    };

    struct FileViewSystem : Assimp::IOSystem {
        crd_nodiscard bool Exists(const char* path) const noexcept override {
            crd_profile_scoped();
            return std::ifstream(path).is_open();
        }

        crd_nodiscard char getOsSeparator() const noexcept override {
            crd_profile_scoped();
            return (char)fs::path::preferred_separator;
        }

        crd_nodiscard Assimp::IOStream* Open(const char* path, const char*) noexcept override {
            crd_profile_scoped();
            return new FileViewStream(dtl::make_file_view(path));
        }

        void Close(Assimp::IOStream* stream) noexcept override {
            crd_profile_scoped();
            delete stream;
        }
    };

    crd_nodiscard static inline Async<StaticTexture>* import_texture(const Context& context, Renderer& renderer, const aiMaterial* material, aiTextureType type, TextureCache& cache, const fs::path& path) noexcept {
        crd_profile_scoped();
        crd_unlikely_if(type == aiTextureType_HEIGHT && material->GetTextureCount(type) == 0) {
            type = aiTextureType_NORMALS;
        }
        crd_unlikely_if(material->GetTextureCount(type) == 0) {
            return nullptr;
        }
        aiString str;
        material->GetTexture(type, 0, &str);
        auto file_name = (path / str.C_Str()).generic_string();
        std::replace(file_name.begin(), file_name.end(), '\\', '/');
        const auto format = type == aiTextureType_DIFFUSE ? texture_srgb : texture_unorm;
        const auto [cached, miss] = cache.try_emplace(file_name);
        crd_unlikely_if(miss) {
            cached->second = new Async<StaticTexture>(request_static_texture(context, renderer, std::move(file_name), format));
        }
        return cached->second;
    }

    crd_nodiscard static inline TexturedMesh import_textured_mesh(const Context& context, Renderer& renderer, const aiScene* scene, const aiMesh* mesh, TextureCache& cache, const fs::path& path) noexcept {
        crd_profile_scoped();
        std::vector<float> geometry;
        geometry.resize(mesh->mNumVertices * vertex_components);
        auto* ptr = &geometry.front();
        for (std::size_t i = 0; i < mesh->mNumVertices; ++i) {
            ptr[0] = mesh->mVertices[i].x;
            ptr[1] = mesh->mVertices[i].y;
            ptr[2] = mesh->mVertices[i].z;

            crd_likely_if(mesh->mNormals) {
                ptr[3] = mesh->mNormals[i].x;
                ptr[4] = mesh->mNormals[i].y;
                ptr[5] = mesh->mNormals[i].z;
            }

            crd_likely_if(mesh->mTextureCoords[0]) {
                ptr[6] = mesh->mTextureCoords[0][i].x;
                ptr[7] = mesh->mTextureCoords[0][i].y;
            }

            crd_likely_if(mesh->mTangents) {
                ptr[8] = mesh->mTangents[i].x;
                ptr[9] = mesh->mTangents[i].y;
                ptr[10] = mesh->mTangents[i].z;
            }

            crd_likely_if(mesh->mBitangents) {
                ptr[11] = mesh->mBitangents[i].x;
                ptr[12] = mesh->mBitangents[i].y;
                ptr[13] = mesh->mBitangents[i].z;
            }
            ptr += vertex_components;
        }

        std::vector<std::uint32_t> indices;
        indices.reserve(mesh->mNumFaces * mesh->mFaces[0].mNumIndices);
        for (std::size_t i = 0; i < mesh->mNumFaces; i++) {
            auto& face = mesh->mFaces[i];
            for (std::size_t j = 0; j < face.mNumIndices; j++) {
                indices.emplace_back(face.mIndices[j]);
            }
        }
        const auto material = scene->mMaterials[mesh->mMaterialIndex];
        const auto vertices_size = mesh->mNumVertices;
        const auto index_size = indices.size();
        return {
            .mesh = request_static_mesh(context, { std::move(geometry), std::move(indices) }),
            .diffuse = import_texture(context, renderer, material, aiTextureType_DIFFUSE, cache, path),
            .normal = import_texture(context, renderer, material, aiTextureType_HEIGHT, cache, path),
            .specular = import_texture(context, renderer, material, aiTextureType_SPECULAR, cache, path),
            .vertices = static_cast<std::uint32_t>(vertices_size),
            .indices = static_cast<std::uint32_t>(index_size)
        };
    }

    static inline void process_node(const Context& context, Renderer& renderer, const aiScene* scene, const aiNode* node, StaticModel& model, TextureCache& cache, fs::path path) noexcept {
        for (std::size_t i = 0; i < node->mNumMeshes; i++) {
            model.submeshes.emplace_back(import_textured_mesh(context, renderer, scene, scene->mMeshes[node->mMeshes[i]], cache, path));
        }

        for (std::size_t i = 0; i < node->mNumChildren; i++) {
            process_node(context, renderer, scene, node->mChildren[i], model, cache, path);
        }
    }

    crd_nodiscard crd_module Async<StaticModel> request_static_model(const Context& context, Renderer& renderer, std::string&& path) noexcept {
        crd_profile_scoped();
        spdlog::info("loading model: \"{}\"", path);
        using task_type = std::packaged_task<StaticModel()>;
        auto* task = new task_type([&context, &renderer, path = std::move(path)]() noexcept -> StaticModel {
            crd_profile_scoped();
            Assimp::Importer importer;
            const auto post_process =
                aiProcess_Triangulate |
                aiProcess_FlipUVs     |
                aiProcess_GenNormals  |
                aiProcess_CalcTangentSpace;
            importer.SetIOHandler(new FileViewSystem());
            const auto scene = importer.ReadFile(path, post_process);
            crd_unlikely_if(!scene || !scene->mRootNode) {
                spdlog::critical("Failed to load model \"{}\", error: {}", path, importer.GetErrorString());
                crd_panic();
            }
            TextureCache cache;
            cache.reserve(128);
            StaticModel model;
            process_node(context, renderer, scene, scene->mRootNode, model, cache, fs::path(path).parent_path());
            spdlog::info("StaticModel \"{}\" was loaded successfully", path);
            return model;
        });
        auto future = task->get_future();
        context.scheduler->AddTask({
            .Function = +[](ftl::TaskScheduler*, void* data) {
                crd_profile_scoped();
                auto* task = static_cast<task_type*>(data);
                (*task)();
                delete task;
            },
            .ArgData = task
        }, ftl::TaskPriority::High);
        return make_async(std::move(future));
    }

    crd_module void StaticModel::destroy() noexcept {
        crd_profile_scoped();
        std::unordered_set<Async<StaticTexture>*> to_destroy;
        to_destroy.reserve(submeshes.size() * 3);
        for (auto& each : submeshes) {
            each.mesh->destroy();
            to_destroy.emplace(each.diffuse);
            to_destroy.emplace(each.normal);
            to_destroy.emplace(each.specular);
        }
        auto empty = to_destroy.find(nullptr);
        if (empty != to_destroy.end()) {
            to_destroy.erase(empty);
        }
        for (auto* each : to_destroy) {
            (*each)->destroy();
            delete each;
        }
        *this = {};
    }
} // namespace crd
