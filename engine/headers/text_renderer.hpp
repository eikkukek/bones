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

		struct GlyphAtlas {
			Character m_Characters[128]{};
			Vec2_T<uint32_t> m_Extent{};
			uint8_t* m_Atlas{};
			uint32_t m_PixelSize{};
			const char* m_FileName{};
			int m_MaxHoriBearingY{};
		};

		struct TextImage {
			Vec2_T<uint32_t> m_Extent;
			uint32_t* m_Image;
		};

		Renderer& m_Renderer;
		FT_Library m_FreeTypeLib;
		const CriticalErrorCallback m_CriticalErrorCallback;

		TextRenderer(Renderer& renderer, size_t maxFonts, CriticalErrorCallback criticalErrorCallback) 
			: m_Renderer(renderer), m_CriticalErrorCallback(criticalErrorCallback) {
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

		const bool CreateGlyphAtlas(const char* fontFileName, uint32_t pixelSize, GlyphAtlas& out) {
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
				return {};
			}
			FT_Set_Pixel_Sizes(face, 0, pixelSize * 16);
			out.m_FileName = fontFileName;
			uint32_t bitmapWidth = 0;
			uint32_t bitmapHeight = 0;
			for (unsigned char c = 0; c < 128; c++) {
				if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
					continue;
				}
				bitmapHeight = Max(bitmapHeight, face->glyph->bitmap.rows);
				out.m_MaxHoriBearingY = Max(((int)face->glyph->metrics.horiBearingY >> 6) / 4, out.m_MaxHoriBearingY);
				bitmapWidth += face->glyph->bitmap.width;
			}
			out.m_Extent = { bitmapWidth / 4, bitmapHeight / 4 };
			size_t atlasPixelCount = out.m_Extent.x * out.m_Extent.y;
			out.m_Atlas = (uint8_t*)malloc(atlasPixelCount * sizeof(uint8_t));
			memset(out.m_Atlas, 0, atlasPixelCount);
			out.m_PixelSize = pixelSize;
			Vec2_T<uint32_t> pen{};
			uint32_t currentPenStartingXPos = 0;
			for (unsigned char c = 0; c < 128; c++) {
				if (!FtCheck(FT_Load_Char(face, c, FT_LOAD_RENDER), 
						"failed to load font character (function FT_Load_Char in function CreateGlyphAtlas)!")) {
					fmt::print(fmt::fg(fmt::color::crimson) | fmt::emphasis::bold, 
						"failed to load character {} from font {}\n", c, fontFileName);
					continue;
				}
				pen.x = currentPenStartingXPos;
				pen.y = 0;
				uint32_t pitch = face->glyph->bitmap.pitch;
				if (face->glyph->bitmap.width > 0) {
					uint8_t* bitmapBuf = face->glyph->bitmap.buffer;
					uint32_t bitmapWidth = face->glyph->bitmap.width;
					uint32_t bitmapHeight = face->glyph->bitmap.rows;
					uint32_t bitmapSize = bitmapWidth * bitmapHeight;
					float nom = 0;
					static constexpr float denom = 16;
					for (uint32_t offsetY = 0; offsetY + 4 < bitmapHeight; offsetY += 4) {
						for (uint32_t offsetX = 0; offsetX + 4 < bitmapWidth; offsetX += 4) {
							for (uint32_t y = 0; y < 4; y++) {
								for (uint32_t x = 0; x < 4; x++) {
									uint32_t byteIndex = (offsetY + y) * bitmapWidth + x + offsetX;
									assert(byteIndex < bitmapSize);
									if (bitmapBuf[byteIndex]) {
										nom++;
									}
								}
							}
							out.m_Atlas[pen.y * out.m_Extent.x + pen.x++] = nom / denom * 255;
							nom = 0.0f;
						}
						pen.y++;
						pen.x = currentPenStartingXPos;
					}
					out.m_Characters[c].m_Size = { bitmapWidth / 4, bitmapHeight / 4 };
					out.m_Characters[c].m_Escapement = { bitmapWidth / 4, 0 };
					out.m_Characters[c].m_Offset = currentPenStartingXPos;
					out.m_MaxHoriBearingY = Max(out.m_MaxHoriBearingY, ((int)face->glyph->metrics.horiBearingY >> 6) / 4);
					currentPenStartingXPos += bitmapWidth / 4;
				}
			}
			FtCheck(FT_Done_Face(face), "failed to terminate FreeType face (function FT_Done_Face in function CreateFontTexture)!");
			return true;
		}

		uint32_t CalcWordWidth(const char* text, size_t pos, uint32_t frameWidth, const GlyphAtlas& atlas, size_t& outEndPos) {
			uint32_t res = 0;
			char c = text[pos];
			size_t i = pos;
			for (; c && c != ' '; i++, c = text[i]) {
				if (c < 0) {
					continue;
				}
				uint32_t temp = res + atlas.m_Characters[c].m_Escapement.x;
				if (temp >= frameWidth) {
					break;
				}
				res = temp;
			}
			outEndPos = i;
			return res;
		}

		uint32_t CalcLineWidth(const char* text, size_t pos, uint32_t frameWidth, const GlyphAtlas& atlas, size_t& outEndPos, bool& wordCut) {
			uint32_t res = 0;
			char c = text[pos];
			size_t i = pos;
			size_t lastWordEnd = i;
			uint32_t lengthAtLastWord = 0;
			uint32_t spaceEscapement = atlas.m_Characters[' '].m_Escapement.x;
			wordCut = false;
			for (; c && c != '\n'; i++, c = text[i]) {
				if (c < 0) {
					continue;
				}
				uint32_t escapement = atlas.m_Characters[c].m_Escapement.x;
				res += escapement;
				if (res >= frameWidth) {
					if (lastWordEnd != pos) {
						i = lastWordEnd;
					}
					else {
						res -= escapement;
						wordCut = true;
					}
					break;
				}
				if (c == ' ') {
					lastWordEnd = i;
					lengthAtLastWord = res - spaceEscapement;
				}
			}
			outEndPos = i;
			if (i && text[i - 1] == ' ') {
				res -= spaceEscapement;
			}
			if (c && i == lastWordEnd) {
				return lengthAtLastWord;
			}	
			return res;
		}
	
		enum class TextAlignment {
			Left = 0,
			Middle = 1,
		};

		template<TextAlignment text_alignment_T = TextAlignment::Left>
		TextImage RenderText(const char* text, const GlyphAtlas& atlas, uint32_t color,
			Vec2_T<uint32_t> frameExtent, Vec2_T<uint32_t> spacing) {
			static_assert(sizeof(uint32_t) == 4, "size of uint32_t was not 4!");
			if (frameExtent.x == 0 || frameExtent.y == 0) {
				PrintError(ErrorOrigin::Uncategorized, "frame size was 0 (in function TextRenderer::RenderText)!");
				return {};
			}
			TextImage res{};
			res.m_Extent = frameExtent;
			size_t resPixelCount = res.m_Extent.x * res.m_Extent.y;
			size_t allocationSize = resPixelCount * 4;
			res.m_Image = (uint32_t*)malloc(allocationSize);
			if (res.m_Image) {
				memset(res.m_Image, 0, allocationSize);
			}
			else {
				PrintError(ErrorOrigin::Uncategorized, "failed to allocate memory (function malloc in function RenderText)!");
				return {};
			}
			size_t textLength = strlen(text);
			uint32_t atlasPixelCount = atlas.m_Extent.x * atlas.m_Extent.y;
			if constexpr (text_alignment_T == TextAlignment::Left) {
				Vec2_T<uint32_t> pen = { spacing.x, spacing.y };
				uint32_t currentPenStartingYPos = spacing.y;
				for (size_t i = 0; i < textLength;) {
					char c = text[i];
					if (c < 0) {
						++i;
						continue;
					}
					if (c == ' ') {
						pen.x += atlas.m_Characters[c].m_Escapement.x;
						++i;
						continue;
					}	
					size_t end;
					uint32_t wordWidth = CalcWordWidth(text, i, frameExtent.x, atlas, end);
					assert(wordWidth < frameExtent.x);
					if (pen.x + wordWidth + spacing.x > frameExtent.x) {
						pen.x = spacing.x;
						currentPenStartingYPos += atlas.m_MaxHoriBearingY + spacing.y;
					}
					for (; i < end; i++) {
						c = text[i];
						if (c == '\n') {
							currentPenStartingYPos += atlas.m_MaxHoriBearingY + spacing.y;
							pen.x = spacing.x;
							++i;
							continue;
						}
						const Character& character = atlas.m_Characters[c];
						uint32_t charWidth = character.m_Size.x;
						uint32_t charHeight = character.m_Size.y;
						pen.y = currentPenStartingYPos;
						pen.y += atlas.m_MaxHoriBearingY - character.m_Bearing.y;
						uint32_t currentPenStartingXPos = pen.x;
						for (size_t y = 0; y < charHeight; y++) {
							if (pen.y > res.m_Extent.y) {
								fmt::print(fmt::fg(fmt::color::yellow) | fmt::emphasis::bold,
									"Text truncated:\n\"{}\" (in function TextRenderer::RenderText)\n", text);
								return res;
							}
							for (size_t x = 0; x < charWidth; x++) {
								size_t imageIndex = pen.y * frameExtent.x + pen.x;
								size_t fontImageIndex = y * atlas.m_Extent.x + character.m_Offset + x;
								assert(imageIndex < resPixelCount && fontImageIndex < atlasPixelCount);
								res.m_Image[imageIndex] = atlas.m_Atlas[fontImageIndex] ? color : 0U;
								++pen.x;
							}
							++pen.y;
							pen.x = currentPenStartingXPos;
						}
						pen.x += character.m_Escapement.x;
					}
				}
			}
			else if (text_alignment_T == TextAlignment::Middle) {
				uint32_t imageWidthHalf = frameExtent.x >> 1;
				Vec2_T<uint32_t> pen(0, spacing.y);
				uint32_t currentPenStartingYPos = pen.y;
				for (size_t i = 0; i < textLength;) {
					char c = text[i];
					if (c < 0) {
						++i;
						continue;
					}
					size_t end;
					bool wordCut;
					size_t lineWidth = CalcLineWidth(text, i, frameExtent.x, atlas, end, wordCut);
					assert(lineWidth < frameExtent.x);
					pen.x = imageWidthHalf - (lineWidth >> 1);
					for (; i < end; i++) {
						c = text[i];
						if (c < 0) {
							continue;
						}
						const Character& character = atlas.m_Characters[c];
						uint32_t charWidth = character.m_Size.x;
						uint32_t charHeight = character.m_Size.y;
						pen.y = currentPenStartingYPos;
						pen.y += spacing.y;
						uint32_t currentPenStartingXPos = pen.x;
						for (size_t y = 0; y < charHeight; y++) {
							if (pen.y >= res.m_Extent.y) {
								fmt::print(fmt::fg(fmt::color::yellow) | fmt::emphasis::bold,
									"Text truncated:\n\"{}\" (in function TextRenderer::RenderText)\n", text);
								return res;
							}
							for (size_t x = 0; x < charWidth; x++) {
								size_t imageIndex = pen.y * frameExtent.x + pen.x;
								size_t fontImageIndex = y * atlas.m_Extent.x + character.m_Offset + x;
								assert(imageIndex < resPixelCount && fontImageIndex < atlasPixelCount);
								res.m_Image[imageIndex] = atlas.m_Atlas[fontImageIndex] ? color : 0U;
								++pen.x;
							}
							++pen.y;
							pen.x = currentPenStartingXPos;
						}
						pen.x += character.m_Escapement.x;
					}
					currentPenStartingYPos += atlas.m_MaxHoriBearingY + spacing.y;
					if (!wordCut) {
						++i;
					}
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

	typedef TextRenderer::GlyphAtlas GlyphAtlas;
	typedef TextRenderer::TextImage TextImage;
	typedef TextRenderer::TextAlignment TextAlignment;
}
