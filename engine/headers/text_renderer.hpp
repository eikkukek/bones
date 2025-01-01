#pragma once

#include "renderer.hpp"
#include "math.hpp"
#include "algorithm.hpp"
#include "freetype/freetype.h"
#include "fmt/printf.h"
#include <future>

namespace engine {

	class TextRenderer {
	public:

		enum class ErrorOrigin {
			Uncategorized = 0,
			FreeType = 1,
			OutOfMemory = 2,
			MaxEnum,
		};

		static const char* ErrorOriginString(ErrorOrigin origin) {
			static constexpr const char* strings[(size_t)ErrorOrigin::MaxEnum] {
				"Uncategorized",
				"FreeType",
				"OutOfMemory",
			};
			if (origin == ErrorOrigin::MaxEnum) {
				return strings[0];
			}
			return strings[(size_t)origin];
		}

		typedef void (*CriticalErrorCallback)(const TextRenderer* renderer, ErrorOrigin origin, const char* err, FT_Error ftErr);

		static void PrintError(ErrorOrigin origin, const char* err, FT_Error ftErr = 0, VkResult vkErr = VK_SUCCESS) {
			fmt::print(fmt::fg(fmt::color::crimson) | fmt::emphasis::bold, 
				"Text renderer called an error!\nError origin: {}\nError: {}\n", ErrorOriginString(origin), err);
			if (ftErr) {
				fmt::print(fmt::fg(fmt::color::crimson) | fmt::emphasis::bold,
					"FreeType error code: {}\n", ftErr);
			}
			if (vkErr != VK_SUCCESS) {
				fmt::print(fmt::fg(fmt::color::crimson) | fmt::emphasis::bold,
					"Vulkan error code: {}\n", (int)vkErr);
			}
		}

		struct Character {
			Vec2_T<uint32_t> m_Size;
			Vec2_T<uint32_t> m_Escapement;
			IntVec2 m_Bearing;
			uint32_t m_Offset;
		};

		struct Font {
			Character m_Characters[128]{};
			Vec2_T<uint32_t> m_ImageExtent{};
			uint8_t* m_Image{};
			uint32_t m_FontSize{};
			const char* m_FileName{};
			int m_MaxHoriBearingY{};
		};

		struct FontList {

			typedef Font* Iterator;
			typedef Font* const ConstIterator;

			std::mutex m_Mutex;
			const size_t m_MaxFonts;
			size_t m_Count;
			Font* const m_Data;
			
			FontList(size_t maxFonts) 
				: m_Mutex(), m_MaxFonts(maxFonts), m_Data((Font*)malloc(maxFonts * sizeof(Font))), m_Count(0) {}

			FontList(const FontList&) = delete;

			~FontList() {
				Iterator iter = m_Data;
				ConstIterator end = &m_Data[m_Count];
				for (; iter != end; iter++) {
					free(iter->m_Image);
				}
				free(m_Data);
			}

			const Font* AddFont(const Font& font) {
				std::lock_guard<std::mutex> lockGuard(m_Mutex);
				if (m_Count >= m_MaxFonts) {
					PrintError(ErrorOrigin::OutOfMemory, "font list was out of memory (in function FontList::AddFont)!");
					return nullptr;
				}
				return &(m_Data[m_Count++] = font);
			}

			Iterator begin() const {
				return m_Data;
			}

			ConstIterator end() const {
				return &m_Data[m_Count];
			}
		};

		struct TextImage {
			Vec2_T<uint32_t> m_Extent;
			uint8_t* m_Image;
		};

		Renderer& m_Renderer;
		FT_Library m_FreeTypeLib;
		FontList m_FontList;
		const CriticalErrorCallback m_CriticalErrorCallback;

		TextRenderer(Renderer& renderer, size_t maxFonts, CriticalErrorCallback criticalErrorCallback) 
			: m_Renderer(renderer), m_FontList(maxFonts), m_CriticalErrorCallback(criticalErrorCallback) {
			FtAssert(FT_Init_FreeType(&m_FreeTypeLib), 
				"failed to initialize FreeType (function FT_Init_FreeType in TextRenderer constructor)!");
		}

		void FtAssert(FT_Error ftErr, const char* err) const {
			if (ftErr) {
				m_CriticalErrorCallback(this, ErrorOrigin::FreeType, err, ftErr);
			}
		}

		bool FtCheck(FT_Error ftErr, const char* err) const {
			if (ftErr) {
				PrintError(ErrorOrigin::FreeType, err, ftErr);
				return false;
			}
			return true;
		}

		/*

		const Font* CreateFontsAsync(size_t fontCount, char** fileNames, uint32_t* fontSizes) {
			Font* res = m_FontList.m_Data;
			size_t mod4 = fontCount % 4;
			switch (mod4) {
				case 1: {
					CreateFont(fileNames[0], fontSizes[0]);
					break;
				}
				case 2: {
					std::future<const Font*> f1 = std::async(&TextRenderer::CreateFont, *this, fileNames[0], fontSizes[0]);
					std::future<const Font*> f2 = std::async(&TextRenderer::CreateFont, *this, fileNames[1], fontSizes[1]);
					if (!f1.get() || !f2.get()) {
						PrintError(ErrorOrigin::Uncategorized, "failed to create some font (in function CreateFontsAsync)!");
						return nullptr;
					}
					break;
				}
				case 3: {
					std::future<const Font*> f1 = std::async(&TextRenderer::CreateFont, *this, fileNames[0], fontSizes[0]);
					std::future<const Font*> f2 = std::async(&TextRenderer::CreateFont, *this, fileNames[1], fontSizes[1]);
					std::future<const Font*> f3 = std::async(&TextRenderer::CreateFont, *this, fileNames[2], fontSizes[2]);
					if (!f1.get() || !f2.get() || !f3.get()) {
						PrintError(ErrorOrigin::Uncategorized, "failed to create some font (in function CreateFontsAsync)!");
						return nullptr;
					}
					break;
				}
			}
			for (size_t i = mod4; i < fontCount; i++) {
				std::future<const Font*> f1 = std::async(&TextRenderer::CreateFont, *this, fileNames[i], fontSizes[i]);
				++i;
				std::future<const Font*> f2 = std::async(&TextRenderer::CreateFont, *this, fileNames[i], fontSizes[i]);
				++i;
				std::future<const Font*> f3 = std::async(&TextRenderer::CreateFont, *this, fileNames[i], fontSizes[i]);
				++i;
				std::future<const Font*> f4 = std::async(&TextRenderer::CreateFont, *this, fileNames[i], fontSizes[i]);
				++i;
				if (!f1.get() || !f2.get() || !f3.get() || !f4.get()) {
					PrintError(ErrorOrigin::Uncategorized, "failed to create some font (in function CreateFontsAsync)!");
					return nullptr;
				}
			}
			return res;
		}
		*/

		const Font* CreateFont(const char* fontFileName, uint32_t fontSize) {
			FT_Face face;
			FT_Error ftRes = FT_New_Face(m_FreeTypeLib, fontFileName, 0, &face);
			if (ftRes) {
				if (ftRes == FT_Err_Unknown_File_Format) {
					PrintError(ErrorOrigin::FreeType, 
						"failed to load font due to unknown file format (function FT_New_Face in function CreateFontTexture)", ftRes);
				}
				else {
					PrintError(ErrorOrigin::FreeType, 
						"failed to open font file (function FT_New_Face in function CreateFontTexture)!", ftRes);
				}
				fmt::print(fmt::fg(fmt::color::crimson) | fmt::emphasis::bold, "failed to load font {}\n", fontFileName);
				return nullptr;
			}
			FT_Set_Pixel_Sizes(face, 0, fontSize);
			Font font { .m_FileName = fontFileName };
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
					fmt::print(fmt::fg(fmt::color::crimson) | fmt::emphasis::bold, 
						"failed to load character {} for font {}\n", c, fontFileName);
					continue;
				}
				bitmapHeight = Max(bitmapHeight, face->glyph->bitmap.rows);
				font.m_Characters[c] = {
					.m_Size { face->glyph->bitmap.width, face->glyph->bitmap.rows },
					.m_Escapement = { (uint32_t)face->glyph->advance.x >> 6, (uint32_t)face->glyph->advance.y >> 6 },
					.m_Bearing = { face->glyph->bitmap_left, face->glyph->bitmap_top },
					.m_Offset = bitmapWidth,
				};
				font.m_MaxHoriBearingY = Max((int)face->glyph->metrics.horiBearingY >> 6, font.m_MaxHoriBearingY);
				uint32_t pitch = face->glyph->bitmap.pitch;
				if (face->glyph->bitmap.width > 0) {
					void* bitmapBuf = face->glyph->bitmap.buffer;
					uint32_t rows = face->glyph->bitmap.rows;
					uint32_t width = face->glyph->bitmap.width;
					CharacterData& characterData = characterDatas[c];
					characterData = { width * rows, new uint8_t[width * rows] };
					memset(characterData.m_Data, 0, characterData.m_Size);
					for (size_t i = 0; i < rows; i++) {
						for (size_t j = 0; j < width; j++) {
							characterDatas[c].m_Data[i * pitch + j] = face->glyph->bitmap.buffer[i * pitch + j];
						}
					}
				}
				bitmapWidth += face->glyph->bitmap.width;
			}

			FtCheck(FT_Done_Face(face), "failed to terminate FreeType face (function FT_Done_Face in function CreateFontTexture)!");
			font.m_ImageExtent = { bitmapWidth, bitmapHeight };
			size_t pixelCount = bitmapWidth * bitmapHeight;
			font.m_Image = (uint8_t*)malloc(pixelCount * sizeof(uint8_t));
			font.m_FontSize = fontSize;
			memset(font.m_Image, 0, pixelCount);
			uint32_t xPos = 0;
			for (unsigned char c = 0; c < 128; c++) {
				Character& character = font.m_Characters[c];
				uint32_t width = character.m_Size.x;
				uint32_t height = character.m_Size.y;
				uint32_t characterPixelCount = width * height;
				for (uint32_t i = 0; i < height; i++) {
					for (uint32_t j = 0; j < width; j++) {
						size_t imageIndex = i * bitmapWidth + xPos + j;
						size_t characterIndex = i * width + j;
						assert(imageIndex < pixelCount && characterIndex < characterPixelCount);
						font.m_Image[imageIndex] = characterDatas[c].m_Data[characterIndex];
					}
				}
				xPos += width;
			}
			return m_FontList.AddFont(font);
		}

		uint32_t CalcWordWidth(const char* text, size_t pos, uint32_t frameWidth, const Font& font, size_t& outEndPos) {
			uint32_t res = 0;
			char c = text[pos];
			size_t i = pos;
			for (; c && c != ' '; c = text[i], i++) {
				if (c < 0) {
					continue;
				}
				uint32_t temp = res + font.m_Characters[c].m_Escapement.x;
				if (temp >= frameWidth) {
					break;
				}
				res = temp;
			}
			outEndPos = i;
			return res;
		}

		TextImage RenderText(const char* text, const Font& font, Vec2_T<uint32_t> frameExtent, Vec2_T<uint32_t> spacing) {
			TextImage res{};
			res.m_Extent = frameExtent;
			if (res.m_Extent.x == 0 || res.m_Extent.y == 0) {
				fmt::print(fmt::fg(fmt::color::crimson) | fmt::emphasis::bold,
					"frame width was smaller than a single character width when rendering text with font {} (in function TextRenderer::RenderText)!",
					font.m_FileName);
				return {};
			}
			uint32_t resPixelCount = res.m_Extent.x * res.m_Extent.y;
			res.m_Image = (uint8_t*)malloc(resPixelCount);
			if (res.m_Image) {
				memset(res.m_Image, 0, res.m_Extent.x * res.m_Extent.y);
			}
			else {
				PrintError(ErrorOrigin::Uncategorized, "failed to allocate memory (function malloc in function RenderText)!");
				return {};
			}
			size_t textLength = strlen(text);
			uint32_t fontPixelCount = font.m_ImageExtent.x * font.m_ImageExtent.y;
			uint32_t imageWidth = res.m_Extent.x;
			Vec2_T<uint32_t> pen = { spacing.x, spacing.y };
			uint32_t currentPenStartingYPos = spacing.y;
			for (size_t i = 0; i < textLength;) {
				char c = text[i];
				if (c < 0) {
					fmt::print(fmt::fg(fmt::color::yellow) | fmt::emphasis::bold,
						"Invalid character in string (in function TextRenderer::RenderText)!\n");
					i++;
					continue;
				}
				if (c == ' ') {
					pen.x += font.m_Characters[c].m_Escapement.x;
					i++;
					continue;
				}
				size_t end;
				uint32_t wordWidth = CalcWordWidth(text, i, frameExtent.x, font, end);
				assert(wordWidth < frameExtent.x);
				if (pen.x + wordWidth + spacing.x > frameExtent.x) {
					pen.x = spacing.x;
					currentPenStartingYPos += font.m_MaxHoriBearingY + spacing.y;
				}
				for (; i < end; i++) {
					c = text[i];
					const Character& character = font.m_Characters[c];
					uint32_t charWidth = character.m_Size.x;
					uint32_t charHeight = character.m_Size.y;
					pen.y = currentPenStartingYPos;
					pen.y += font.m_MaxHoriBearingY - character.m_Bearing.y;
					uint32_t currentPenStartingXPos = pen.x;
					for (size_t y = 0; y < charHeight; y++) {
						if (pen.y > res.m_Extent.y) {
							fmt::print(fmt::fg(fmt::color::yellow) | fmt::emphasis::bold,
								"Text truncated:\n\"{}\" (in function TextRenderer::RenderText)\n", text);
							return res;
						}
						for (size_t x = 0; x < charWidth; x++) {
							size_t imageIndex = pen.y * imageWidth + pen.x;
							size_t fontImageIndex = y * font.m_ImageExtent.x + character.m_Offset + x;
							assert(imageIndex < resPixelCount && fontImageIndex < fontPixelCount);
							res.m_Image[imageIndex] = font.m_Image[fontImageIndex];
							++pen.x;
						}
						++pen.y;
						pen.x = currentPenStartingXPos;
					}
					pen.x += character.m_Escapement.x;
				}
			}
			return res;
		}

		void DestroyTextImage(TextImage& image) {
			free(image.m_Image);
			image.m_Image = nullptr;
			image.m_Extent = {};
		}
	};

	typedef TextRenderer::Font Font;
	typedef TextRenderer::TextImage TextImage;
}
