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
			unsigned char* m_Atlas{};
			uint32_t m_FontSize{};
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

		TextRenderer(Renderer& renderer, CriticalErrorCallback criticalErrorCallback) 
			: m_Renderer(renderer), m_CriticalErrorCallback(criticalErrorCallback)  {
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

		const bool CreateGlyphAtlas(const char* fontFileName, uint32_t fontPixelSize, GlyphAtlas& out) {
			assert(fontPixelSize > 0);
			FT_Face face;
			FT_Error ftRes = FT_New_Face(m_FreeTypeLib, fontFileName, 0, &face);
			if (ftRes) {
				if (ftRes == FT_Err_Unknown_File_Format) {
					PrintError(ErrorOrigin::FreeType, 
						"failed to load font due to unknown file format (function FT_New_Face in function CreateGlyphAtlas)", ftRes);
				}
				else {
					PrintError(ErrorOrigin::FreeType, 
						"failed to open font file (function FT_New_Face in function CreateGlyphAtlas)!", ftRes);
				}
				fmt::print(fmt::fg(fmt::color::crimson) | fmt::emphasis::bold, "failed to load font {}\n", fontFileName);
				return {};
			}
			FT_Set_Pixel_Sizes(face, 0, fontPixelSize);
			out.m_FileName = fontFileName;
			uint32_t sampledImageWidth = 0;
			uint32_t sampledImageHeight = 0;
			unsigned char* bitmaps[128]{};
			for (unsigned char c = 0; c < 128; c++) {
				if (!FtCheck(FT_Load_Char(face, c, FT_LOAD_RENDER), 
						"failed to load font character (function FT_Load_Char in function CreateGlyphAtlas)!")) {
					fmt::print(fmt::fg(fmt::color::crimson) | fmt::emphasis::bold, 
						"failed to load character {} from font {}\n", c, fontFileName);
					continue;
				}
				sampledImageHeight = Max(sampledImageHeight, face->glyph->bitmap.rows);
				out.m_MaxHoriBearingY = Max(((int)face->glyph->metrics.horiBearingY >> 6), out.m_MaxHoriBearingY);
				Vec2_T<uint32_t> bitmapSize(face->glyph->bitmap.width, face->glyph->bitmap.rows);
				out.m_Characters[c] = {
					.m_Size = bitmapSize,
					.m_Escapement { (uint32_t)face->glyph->advance.x >> 6, (uint32_t)face->glyph->advance.y >> 6 },
					.m_Bearing { face->glyph->bitmap_left, face->glyph->bitmap_top },
					.m_Offset = sampledImageWidth,
				};
				sampledImageWidth += face->glyph->bitmap.width;
				size_t pixelCount = bitmapSize.x * bitmapSize.y;
				if (pixelCount == 0) {
					continue;
				}
				size_t allocSize = pixelCount * sizeof(unsigned char);
				unsigned char* bitmap = bitmaps[c] = (unsigned char*)malloc(allocSize);
				unsigned char* buf = face->glyph->bitmap.buffer;
				for (size_t y = 0; y < bitmapSize.y; y++) {
					for (size_t x = 0; x < bitmapSize.x; x++) {
						size_t index = y * bitmapSize.x + x;
						assert(index < pixelCount);
						bitmap[index] = buf[index];
					}
				}
			}
			FtCheck(FT_Done_Face(face), "failed to terminate FreeType face (function FT_Done_Face in function CreateGlyphAtlas)!");
			out.m_Extent = { sampledImageWidth, sampledImageHeight };
			size_t atlasPixelCount = out.m_Extent.x * out.m_Extent.y;
			size_t atlasAllocSize = atlasPixelCount * sizeof(unsigned char);
			out.m_Atlas = (unsigned char*)malloc(atlasAllocSize);
			if (out.m_Atlas) {
				memset(out.m_Atlas, 0, atlasAllocSize);
			}
			else {
				PrintError(ErrorOrigin::Uncategorized, "failed to allocate glyph atlas image (function malloc in function CreateGlyphAtlas)!");
				return false;
			}
			out.m_FontSize = fontPixelSize;
			Vec2_T<uint32_t> pen{};
			uint32_t currentPenStartingXPos = 0;
			for (unsigned char c = 0; c < 128; c++) {
				pen.x = currentPenStartingXPos;
				pen.y = 0;
				unsigned char* bitmap = bitmaps[c];
				if (bitmap) {
					Character& character = out.m_Characters[c];
					uint32_t bitmapWidth = character.m_Size.x;
					uint32_t bitmapHeight = character.m_Size.y;
					uint32_t bitmapPixelCount = bitmapWidth * bitmapHeight;
					for (uint32_t y = 0; y < bitmapHeight; y++) {
						for (uint32_t x = 0; x < bitmapWidth; x++) {
							uint32_t bitmapIndex = y * bitmapWidth + x;
							uint32_t atlasIndex = pen.y * out.m_Extent.x + pen.x;
							assert(bitmapIndex < bitmapPixelCount && atlasIndex < atlasPixelCount);
							out.m_Atlas[atlasIndex] = bitmap[bitmapIndex];
							pen.x++;
						}
						pen.x = currentPenStartingXPos;
						pen.y++;
					}
					currentPenStartingXPos += bitmapWidth;
				}
				free(bitmap);
			}
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

		static constexpr inline uint8_t ClampComponent(uint32_t comp) {
			return comp & ~255 ? 255 : (uint8_t)comp;
		}

		static constexpr inline uint8_t GetComponentRGBA(uint32_t color, uint32_t component) {
			return (uint8_t)(color << 8 * (3 - component) >> 24);
		}

		static constexpr inline uint32_t BlendTextColorRGBA(uint32_t t, uint32_t bg) {
			uint8_t bgA = bg >> 24;
			if (!(bg >> 24)) {
				return t;
			}
			uint8_t tA = t >> 24;	
			return ClampComponent((GetComponentRGBA(t, 0) * tA + GetComponentRGBA(bg, 0) * (255 - tA)) / 255)
				+ (ClampComponent((GetComponentRGBA(t, 1) * tA + GetComponentRGBA(bg, 1) * (255 - tA)) / 255) << 8)
				+ (ClampComponent((GetComponentRGBA(t, 2) * tA + GetComponentRGBA(bg, 2) * (255 - tA)) / 255) << 16)
				+ (ClampComponent(bgA + tA) << 24);
		}

		struct RenderTextInfo {
			const GlyphAtlas& m_GlyphAtlas;
			Vec2_T<uint32_t> m_Spacing;
			uint32_t m_TextColor;
			uint32_t m_BackGroundColor;
		};

		template<TextAlignment text_alignment_T = TextAlignment::Left>
		TextImage RenderText(const char* text, const RenderTextInfo& renderInfo, Vec2_T<uint32_t> frameExtent, uint32_t* bgImage = nullptr) {
			static_assert(sizeof(uint32_t) == 4, "size of uint32_t was not 4!");
			static_assert(sizeof(uint8_t) == 1, "size of uin8_t was not 1!");
			static_assert(sizeof(uint8_t) == sizeof(unsigned char), "size of unsigned char was not size of uint8_t!");
			if (frameExtent.x == 0 || frameExtent.y == 0) {
				PrintError(ErrorOrigin::Uncategorized, "frame size was 0 (in function TextRenderer::RenderText)!");
				return {};
			}
			const GlyphAtlas& atlas = renderInfo.m_GlyphAtlas;
			Vec2_T<uint32_t> spacing = renderInfo.m_Spacing;
			TextImage res{};
			res.m_Extent = frameExtent;
			size_t resPixelCount = res.m_Extent.x * res.m_Extent.y;
			size_t allocationSize = resPixelCount * 4;
			res.m_Image = (uint32_t*)malloc(allocationSize);
			if (res.m_Image) {
				if (bgImage) {
					memcpy(res.m_Image, bgImage, allocationSize);
				}
				else if (renderInfo.m_BackGroundColor) {
					for (size_t i = 0; i < resPixelCount; i++) {
						res.m_Image[i] = renderInfo.m_BackGroundColor;
					}
				}
				else {
					memset(res.m_Image, 0, allocationSize);
				}
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
								size_t atlasIndex = y * atlas.m_Extent.x + character.m_Offset + x;
								size_t resImageIndex = pen.y * frameExtent.x + pen.x;
								assert(atlasIndex < atlasPixelCount && resImageIndex < resPixelCount);
								float val = (float)atlas.m_Atlas[atlasIndex] / 255;
								if (val != 0.0f) {
									const uint8_t* pColor = (uint8_t*)&renderInfo.m_TextColor;
									uint8_t trueColor[4] {
										(uint8_t)(val * pColor[0]),
										(uint8_t)(val * pColor[1]),
										(uint8_t)(val * pColor[2]),
										(uint8_t)(val * pColor[3]),
									};
									uint32_t& bgCol = res.m_Image[resImageIndex];
									bgCol = BlendTextColorRGBA(*(uint32_t*)trueColor, bgCol);
								}
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
						pen.y = currentPenStartingYPos + atlas.m_MaxHoriBearingY - character.m_Bearing.y;
						uint32_t currentPenStartingXPos = pen.x;
						for (size_t y = 0; y < charHeight; y++) {
							if (pen.y >= res.m_Extent.y) {
								fmt::print(fmt::fg(fmt::color::yellow) | fmt::emphasis::bold,
									"Text truncated:\n\"{}\" (in function TextRenderer::RenderText)\n", text);
								return res;
							}
							for (size_t x = 0; x < charWidth; x++) {
								size_t resImageIndex = pen.y * frameExtent.x + pen.x;
								size_t atlasImageIndex = y * atlas.m_Extent.x + character.m_Offset + x;
								assert(resImageIndex < resPixelCount && atlasImageIndex < atlasPixelCount);
								float val = (float)atlas.m_Atlas[atlasImageIndex] / 255;
								if (val != 0.0f) {
									const uint8_t* pColor = (uint8_t*)&renderInfo.m_TextColor;
									uint8_t trueColor[4] {
										(uint8_t)(val * pColor[0]),
										(uint8_t)(val * pColor[1]),
										(uint8_t)(val * pColor[2]),
										(uint8_t)(val * pColor[3]),
									};
									uint32_t& bgCol = res.m_Image[resImageIndex];
									bgCol = BlendTextColorRGBA(*(uint32_t*)trueColor, bgCol);
								}
								++pen.x;
							}
							++pen.y;
							pen.x = currentPenStartingXPos;
						}
						pen.x += character.m_Escapement.x;
					}
					currentPenStartingYPos += atlas.m_MaxHoriBearingY + spacing.y;
					i++;
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
