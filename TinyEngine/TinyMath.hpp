#pragma once
#ifndef TINY_ENGOME_TINYMATH
#define TINY_ENGOME_TINYMATH
    #include "./TinyEngine.hpp"

    namespace TINY_ENGINE_NAMESPACE {
        /// @brief Default math coordinate and 2D camera projection functionality.
        class TinyMath {
	    public:
            const static glm::mat4 Project2D(double width, double height, double camerax, double cameray, double znear = 1.0, double zfar = 0.0) {
                //return glm::transpose(glm::mat4(
                //    2.0/(width - 1), 0.0, 0.0, -1.0,
                //    0.0, 2.0/(height-1), 0.0, -1.0,
                //    0.0, 0.0, -2.0/(zfar-znear), -((zfar+znear)/(znear-zfar)),
                //    0.0, 0.0, 0.0, 1.0));
                // Defining the TOP and BOTTOM upside down will provide the proper translation transform for scaling
                // with Vulkan due to Vulkan's inverted Y-Axis without having to transpose the matrix.
                glm::mat4 projection = glm::ortho(0.0, width, 0.0, height, znear, zfar);
                return glm::translate(projection, glm::vec3(camerax, cameray, 0.0));

            }
            
            const static glm::vec2 GetUVCoords(glm::vec2 xy, glm::vec2 wh, bool forceClamp = true) {
                if (forceClamp)
                    xy = glm::clamp(xy, glm::vec2(0.0, 0.0), wh);
                return xy * (glm::vec2(1.0, 1.0) / wh);
            }

            const static glm::vec2 GetXYCoords(glm::vec2 uv, glm::vec2 wh, bool forceClamp = true) {
                if (forceClamp)
                    uv = glm::clamp(uv, glm::vec2(0.0, 0.0), glm::vec2(1.0, 1.0));

                return glm::vec2(uv.x * wh.x, uv.y * wh.y);
            }

            const static glm::float32 AngleClamp(glm::float32 a) {
                #ifndef GLM_FORCE_RADIANS
                return std::fmod((360.0f + std::fmod(a, 360.0f)), 360.0f);
                #else
                constexpr glm::float32 pi2 = glm::pi<glm::float32>() * 2.0f;
                return std::fmod((pi2 + std::fmod(a, pi2)), pi2);
                #endif
            }

            const static glm::float32 AngleDelta(glm::float32 a, glm::float32 b) {
                //// https://gamedev.stackexchange.com/a/4472
                glm::float32 absa, absb;
                #ifndef GLM_FORCE_RADIANS
                absa = std::fmod((360.0f + std::fmod(a, 360.0f)), 360.0f);
                absb = std::fmod((360.0f + std::fmod(b, 360.0f)), 360.0f);
                glm::float32 delta = glm::abs(absa - absb);
                glm::float32 sign = absa > absb || delta >= 180.0f ? -1.0f : 1.0f;
                return (180.0f - glm::abs(delta - 180.0f) * sign;
                #else
                constexpr glm::float32 pi = glm::pi<glm::float32>();
                constexpr glm::float32 pi2 = pi * 2.0f;
                absa = std::fmod((pi2 + std::fmod(a, pi2)), pi2);
                absb = std::fmod((pi2 + std::fmod(b, pi2)), pi2);
                glm::float32 delta = glm::abs(absa - absb);
                glm::float32 sign = (absa > absb || delta >= pi) ? -1.0f : 1.0f;
                return (pi - glm::abs(delta - pi)) * sign;
                #endif
            }
	    
			template<typename T>
			static size_t GetSizeofVector(std::vector<T> vector) { return vector.size() * sizeof(T); }

			template<typename T, size_t S>
			static size_t GetSizeofArray(std::array<T,S> array) { return S * sizeof(T); }
        };

        /// @brief Creates non-indexed quads in the format of std::vector of TinyVertex.
        class TinyQuad {
        public:
            static const std::vector<glm::vec4> defvcolors;
            
            static std::vector<TinyVertex> CreateFromAtlas(glm::vec4 xywh, glm::float32 depth, glm::vec4 atlas_xywh, glm::vec2 atlas_wh, const std::vector<glm::vec4> vcolors = defvcolors) {
                glm::vec2 uv1 = { atlas_xywh.x / atlas_wh.x, atlas_xywh.y / atlas_wh.y };
                glm::vec2 uv2 = uv1 + glm::vec2(atlas_xywh.z / atlas_wh.x, atlas_xywh.w / atlas_wh.y);
                glm::vec2 xy1 = glm::vec2(xywh.x, xywh.y);
                glm::vec2 xy2 = glm::vec2(xywh.x, xywh.y) + glm::vec2(xywh.z, xywh.w);

                return {
                    TinyVertex({uv1.x, uv1.y}, {xy1.x, xy1.y, depth}, vcolors[0]),
                    TinyVertex({uv2.x, uv1.y}, {xy2.x, xy1.y, depth}, vcolors[1]),
                    TinyVertex({uv2.x, uv2.y}, {xy2.x, xy2.y, depth}, vcolors[2]),
                    TinyVertex({uv1.x, uv2.y}, {xy1.x, xy2.y, depth}, vcolors[3])
                };
            }

            static std::vector<TinyVertex> Create(glm::vec4 xywh, glm::float32 depth, const std::vector<glm::vec4> vcolors = defvcolors) {
                return CreateFromAtlas(xywh, depth, glm::vec4(0.0, 0.0, 1.0, 1.0), glm::vec2(1.0, 1.0), vcolors);
            }
            
            static std::vector<glm::vec4> CreateVertexColors(glm::vec4 TL, glm::vec4 TR, glm::vec4 BL, glm::vec4 BR) {
                return {TL, TR, BR, BL};
            }
            
            static glm::vec2 GetQuadAtlasXYWH(std::vector<TinyVertex>& quad) {
                glm::vec2 wh = quad[2].texcoord - quad[0].texcoord;
                return glm::vec4(quad[0].texcoord.x, quad[0].texcoord.y, wh.x, wh.y);
            }

            static glm::vec4 GetQuadXYWH(std::vector<TinyVertex>& quad) {
                glm::vec2 wh = quad[2].position - quad[0].position;
                return glm::vec4(quad[0].position.x, quad[0].position.y, wh.x, wh.y);
            }
            
            static void RotateScaleFromOrigin(std::vector<TinyVertex>& quad, glm::vec3 origin, glm::float32 radians, glm::float32 scale) {
                glm::mat2 rotation = glm::mat2(glm::cos(radians), -glm::sin(radians), glm::sin(radians), glm::cos(radians));
                glm::vec2 pivot = origin;
                glm::vec2 position;

                for (size_t i = 0; i < quad.size(); i++) {
                    position = quad[i].position;

                    position -= pivot * scale;
                    position = rotation * scale * position;
                    position += pivot * scale;

                    quad[i].position = glm::vec3(position, quad[i].position.z);
                }
            }

            static void Reposition(std::vector<TinyVertex>& quad, glm::vec2 xy, bool relative) {
                if (relative) {
                    quad[0].position += glm::vec3(xy,0.0f);
                    quad[1].position += glm::vec3(xy,0.0f);
                    quad[2].position += glm::vec3(xy,0.0f);
                    quad[3].position += glm::vec3(xy,0.0f);
                } else {
                    glm::vec2 wh = glm::vec2(quad[1].position.x - quad[0].position.x, quad[2].position.y - quad[3].position.y);
                    quad[0].position = glm::vec3(xy, quad[0].position.z);
                    quad[1].position = glm::vec3(xy.x + wh.x, xy.y, quad[1].position.z);
                    quad[2].position = glm::vec3(xy.x + wh.x, xy.y + wh.y, quad[2].position.z);
                    quad[3].position = glm::vec3(xy.x, xy.y + wh.y, quad[3].position.z);
                }
            }
        };

        /// @brief Default (white) color layout for generating quads.
        const std::vector<glm::vec4> TinyQuad::defvcolors = { {1.0,1.0,1.0,1.0},{1.0,1.0,1.0,1.0},{1.0,1.0,1.0,1.0},{1.0,1.0,1.0,1.0} };
    }
#endif