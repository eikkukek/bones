#pragma once

#include "renderer.hpp"
#include "math.hpp"
#include "algorithm.hpp"
#include "freetype/freetype.h"
#include "fmt/printf.h"

namespace engine {

	class TextRenderer {
	public:

		enum class ErrorOrigin {
			Uncategorized = 0,
			FreeType = 1,
			Vulkan = 2,
			MaxEnum,
		};

		static const char* ErrorOriginString(ErrorOrigin origin) {
			static constexpr const char* strings[(size_t)ErrorOrigin::MaxEnum] {
				"Uncategorized",
				"FreeType",
				"Vulkan",
			};
			if (origin == ErrorOrigin::MaxEnum) {
				return strings[0];
			}
			return strings[(size_t)origin];
		}

		typedef void (*CriticalErrorCallback)(const TextRenderer* renderer, ErrorOrigin origin, const char* err, FT_Error ftErr);

		static void PrintError(ErrorOrigin origin, const char* err, FT_Error ftErr = 0) {
			fmt::print(fmt::fg(fmt::color::crimson) | fmt::emphasis::bold, 
				"Text renderer called an error!\nError origin: {}\nError: {}\n", ErrorOriginString(origin), err);
			if (ftErr) {
				fmt::print("Vulkan error code: {}\n", ftErr); 
			}
		}

		struct Character {
			Vec2_T<uint32_t> m_Size;
			Vec2_T<int> m_Bearing;
			uint32_t m_Offset;
			uint32_t m_Advance;
		};

		Renderer& m_Renderer;
		FT_Library m_FreeTypeLib;
		const CriticalErrorCallback m_CriticalErrorCallback;

		void FtAssert(FT_Error ftErr, const char* err) {
			if (ftErr) {
				m_CriticalErrorCallback(this, ErrorOrigin::FreeType, err, ftErr);
			}
		}

		bool FtCheck(FT_Error ftErr, const char* err) {
			if (ftErr) {
				PrintError(ErrorOrigin::FreeType, err, ftErr);
				return false;
			}
			return true;
		}

		TextRenderer(Renderer& renderer, CriticalErrorCallback criticalErrorCallback) 
			: m_Renderer(renderer), m_CriticalErrorCallback(criticalErrorCallback) {
			FtAssert(FT_Init_FreeType(&m_FreeTypeLib), 
				"failed to initialize FreeType (function FT_Init_FreeType in TextRenderer constructor)!");
		}

		struct Font {
		};

		struct FontTexture {
			VkImage m_Image{};
			VkImageView m_ImageView{};
			VkExtent2D m_Extent{};
			Character m_Characters[128]{};
		};

		VkExtent2D CalculateExtent(uint32_t fontSize) {
		}

		FontTexture CreateFontTexture(const char* fontFileName, uint32_t fontSize) {
			FontTexture texture{};
			texture.m_Extent = CalculateExtent(fontSize);
			FT_Face face;
			if (!FtCheck(FT_New_Face(m_FreeTypeLib, fontFileName, 0, &face), 
					"failed to load font (function FT_New_Face in function CreateFontTexture)!")) {
				fmt::print("font that failed to load: {}\n", fontFileName);
				return {};
			}
			FT_Set_Pixel_Sizes(face, 0, fontSize);
			uint32_t bitmapWidth = 0;
			uint32_t bitmapHeight = 0;
			struct CharacterData {
				uint32_t m_Size;
				uint8_t* m_Data;
			};
			CharacterData characterDatas[128]{};
			for (unsigned char c = 0; c < 128; c++) {
				if (!FtCheck(FT_Load_Char(face, c, FT_LOAD_RENDER), 
						"failed to load font character (function FT_Load_Char in function CreateFontTexture)!")) {
					continue;
				}
				bitmapHeight = Max(bitmapHeight, face->glyph->bitmap.rows);
				texture.m_Characters[c] = {
					.m_Size { face->glyph->bitmap.width, face->glyph->bitmap.rows },
					.m_Bearing { face->glyph->bitmap_left, face->glyph->bitmap_top },
					.m_Offset = bitmapWidth,
					.m_Advance = (uint32_t)face->glyph->advance.x,
				};
				uint32_t pitch = face->glyph->bitmap.pitch;
				if (face->glyph->bitmap.width > 0) {
					void* bitmapBuf = face->glyph->bitmap.buffer;
					uint32_t rows = face->glyph->bitmap.rows;
					uint32_t width = face->glyph->bitmap.width;
					characterDatas[c] = { width * rows, new uint8_t[width * rows] };
					for (size_t i = 0; i < rows; i++) {
						for (size_t j = 0; j < width; j++) {
							characterDatas[c].m_Data[i * pitch + j] = face->glyph->bitmap.buffer[i * pitch + j];
						}
					}
				}
				bitmapWidth += face->glyph->bitmap.width;
			}

			FtCheck(FT_Done_Face(face), "failed to terminate FreeType face (function FT_Done_Face in function CreateFontTexture)!");
			uint8_t* texImage = new uint8_t[bitmapWidth * bitmapHeight]{};
			uint32_t xPos = 0;
			for (unsigned char c = 0; c < 128; c++) {
				Character& character = texture.m_Characters[c];
				uint32_t width = character.m_Size.x;
				uint32_t height = character.m_Size.y;
				for (uint32_t i = 0; i < height; i++) {
					for (uint32_t j = 0; j < width; j++) {
						texImage[i * bitmapWidth + xPos + j] = characterDatas[c].m_Data[i * width + j];
					}
				}
				xPos += width;
			}
			return texture;
		}

		void DestroyTexture(FontTexture& texture) {
			vkDestroyImageView(m_Renderer.m_VulkanDevice, texture.m_ImageView, m_Renderer.m_VulkanAllocationCallbacks);
			texture.m_ImageView = VK_NULL_HANDLE;
			vkDestroyImage(m_Renderer.m_VulkanDevice, texture.m_Image, m_Renderer.m_VulkanAllocationCallbacks);
			texture.m_Image = VK_NULL_HANDLE;
			texture.m_Extent = {};
		}
	};

	typedef TextRenderer::FontTexture FontTexture;
}
