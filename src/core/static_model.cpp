#include <corundum/core/static_texture.hpp>
#include <corundum/core/static_model.hpp>
#include <corundum/core/static_mesh.hpp>
#include <corundum/core/context.hpp>

#include <corundum/detail/file_view.hpp>
#include <corundum/detail/logger.hpp>

#include <assimp/DefaultIOStream.h>
#include <assimp/DefaultIOSystem.h>
#include <assimp/postprocess.h>
#include <assimp/IOSystem.hpp>
#include <assimp/IOStream.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>

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
        detail::FileView handle;
        std::size_t ptr;

        FileViewStream() noexcept = default;

        explicit FileViewStream(detail::FileView handle) noexcept
            : handle(handle),
              ptr() {}

        ~FileViewStream() noexcept override {
            detail::destroy_file_view(handle);
        }

        crd_nodiscard std::size_t Read(void* buffer, std::size_t size, std::size_t count) noexcept override {
            crd_unlikely_if(ptr == handle.size) {
                return 0;
            }
            auto bytes = size * count;
            crd_unlikely_if(ptr + bytes >= handle.size) {
                bytes = handle.size - ptr;
            }
            std::memcpy(buffer, static_cast<const char*>(handle.data) + ptr, bytes);
            ptr += bytes;
            return bytes;
        }

        crd_nodiscard std::size_t Write(const void*, std::size_t size, std::size_t count) noexcept override {
            return size * count;
        }

        crd_nodiscard aiReturn Seek(std::size_t offset, aiOrigin origin) noexcept override {
            switch (origin) {
                case aiOrigin_SET: { ptr  = offset; } break;
                case aiOrigin_CUR: { ptr += offset; } break;
                case aiOrigin_END: { ptr -= offset; } break;
            }
            return aiReturn_SUCCESS;
        }

        crd_nodiscard std::size_t Tell() const noexcept override {
            return ptr;
        }

        crd_nodiscard std::size_t FileSize() const noexcept override {
            return handle.size;
        }

        void Flush() noexcept override {}
    };

    struct FileViewSystem : Assimp::IOSystem {
        crd_nodiscard bool Exists(const char* path) const noexcept override {
            return std::ifstream(path).is_open();
        }

        crd_nodiscard char getOsSeparator() const noexcept override {
            return Assimp::DefaultIOSystem().getOsSeparator();
        }

        crd_nodiscard Assimp::IOStream* Open(const char* path, const char* mode) noexcept override {
            return new FileViewStream(detail::make_file_view(path));
        }

        void Close(Assimp::IOStream* stream) noexcept override {
            delete stream;
        }
    };

    crd_nodiscard static inline Async<StaticTexture>* import_texture(const Context& context, const aiMaterial* material, aiTextureType type, TextureCache& cache, const fs::path& path) noexcept {
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
        crd_likely_if(miss) {
            cached->second = new Async<StaticTexture>(request_static_texture(context, std::move(file_name), format));
        }
        return cached->second;
    }

    crd_nodiscard static inline TexturedMesh import_textured_mesh(const Context& context, const aiScene* scene, const aiMesh* mesh, TextureCache& cache, const fs::path& path) noexcept {
        constexpr auto components = 14;
        std::vector<float> geometry;
        geometry.resize(mesh->mNumVertices * components);
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
            ptr += components;
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

    static inline void process_node(const Context& context, const aiScene* scene, const aiNode* node, StaticModel& model, TextureCache& cache, fs::path path) noexcept {
        for (std::size_t i = 0; i < node->mNumMeshes; i++) {
            model.submeshes.emplace_back(import_textured_mesh(context, scene, scene->mMeshes[node->mMeshes[i]], cache, path));
        }

        for (std::size_t i = 0; i < node->mNumChildren; i++) {
            process_node(context, scene, node->mChildren[i], model, cache, path);
        }
    }

    crd_nodiscard crd_module Async<StaticModel> request_static_model(const Context& context, std::string&& path) noexcept {
        detail::log("Core", detail::severity_info, detail::type_general, "loading model: \"%s\"", path.c_str());
        using task_type = std::packaged_task<StaticModel()>;
        auto* task = new task_type([&context, path = std::move(path)]() noexcept -> StaticModel {
            Assimp::Importer importer;
            const auto post_process =
                aiProcess_Triangulate |
                aiProcess_FlipUVs     |
                aiProcess_GenNormals  |
                aiProcess_CalcTangentSpace;
            importer.SetIOHandler(new FileViewSystem());
            const auto scene = importer.ReadFile(path, post_process);
            crd_assert(scene && !scene->mFlags && scene->mRootNode, "failed to load model");
            TextureCache cache;
            cache.reserve(128);
            StaticModel model;
            process_node(context, scene, scene->mRootNode, model, cache, fs::path(path).parent_path());
            detail::log("Vulkan", detail::severity_info, detail::type_general, "StaticModel \"%s\" was loaded successfully", path.c_str());
            return model;
        });
        context.scheduler->AddTask({
            .Function = +[](ftl::TaskScheduler*, void* data) {
                auto* task = static_cast<task_type*>(data);
                crd_benchmark("time took to upload StaticModel resource: %llums", *task);
                delete task;
            },
            .ArgData = task
        }, ftl::TaskPriority::High);
        return make_async(task->get_future());
    }

    crd_module void destroy_static_model(const Context& context, StaticModel& model) noexcept {
        std::unordered_set<Async<StaticTexture>*> to_destroy;
        to_destroy.reserve(model.submeshes.size() * 3);
        for (auto& each : model.submeshes) {
            destroy_static_mesh(context, *each.mesh);
            to_destroy.emplace(each.diffuse);
            to_destroy.emplace(each.normal);
            to_destroy.emplace(each.specular);
        }
        auto empty = to_destroy.find(nullptr);
        if (empty != to_destroy.end()) {
            to_destroy.erase(empty);
        }
        for (auto* each : to_destroy) {
            destroy_static_texture(context, **each);
            delete each;
        }
        model = {};
    }
} // namespace crd
