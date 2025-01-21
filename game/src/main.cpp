#include "engine.hpp"
#include "random.hpp"
#include <chrono>

class Player {
public:

	static inline Player* s_Instance = nullptr;
	
	engine::World& m_World;
	engine::StaticMesh& m_Mesh;
	engine::Quaternion m_Rotation;
	engine::Engine::PersistentReference<engine::Creature> m_Creature;
	engine::Engine::PersistentReference<engine::World::RenderData> m_RenderData;

	static engine::Vec3 MovementVectorUpdate(const engine::Creature& creature) {
		using namespace engine;
		using Key = Input::Key;
		return Vec3(
				Input::ReadKeyValue(Key::D) - Input::ReadKeyValue(Key::A),
				0.0f,
				Input::ReadKeyValue(Key::W) - Input::ReadKeyValue(Key::S)) * (5 * Time::DeltaTime());
	}

	static void MoveCallback(const engine::Creature& creature, const engine::Vec3& position, const engine::Vec3& deltaPosition) {
		using namespace engine;
		s_Instance->m_RenderData.m_Val->m_Transform[3] = Vec4(position, 1.0f);
	}

	static void CameraFollowCallback(const engine::Creature& creature, engine::Vec3& outCameraPosition, engine::Vec3& outCameraLookAt) {
		using namespace engine;
		const engine::Vec3& creaturePos = creature.GetPosition();
		static const Vec3 cameraOffset(20.0f, 20.0f, -20.0f);
		static const Vec3 lookAtOffset(0.0f, 0.0f, 10.0f);
		outCameraPosition = creaturePos + cameraOffset;
		outCameraLookAt = creaturePos + lookAtOffset;
	}

	Player(engine::World& world, engine::StaticMesh& mesh) 
		: m_World(world), 
			m_Creature(world.AddCreature({ 3.0f, 0.0f, 0.0f }, 
				{ 
					.m_LocalPosition {}, 
					.m_Type { engine::Collider::Type::Pole },
					.u_TypeInfo { .m_PoleInfo { .m_Radius = 1, .m_Height = 2 } },
				}
			)), 
			m_Mesh(mesh), m_RenderData(world.AddRenderData(*m_Creature, {}, {})) {
		using namespace engine;
		s_Instance = this;
		World::RenderData& renderData = *m_RenderData;
		renderData.m_Transform = engine::Mat4(1);
		renderData.m_Transform[3].z = 3;
		renderData.m_MeshData = m_Mesh.GetMeshData();
		Creature& creature = *m_Creature;
		creature.m_MovementVectorUpdate = MovementVectorUpdate;
		creature.m_MoveCallback = MoveCallback;
		creature.m_CameraFollowCallback = CameraFollowCallback;
		world.SetCameraFollowCreature(*m_Creature);
	}	
};

/*

class NPC {
public:

	static inline NPC* s_Instance = nullptr;

	engine::World& m_World;
	engine::StaticMesh& m_Mesh;
	engine::Engine::Reference<engine::Creature> m_Creature;

	NPC(engine::World& world, engine::StaticMesh& mesh)
		: m_World(world), m_Creature(world.AddCreature(engine::Vec3(0.0f, 0.0f, 0.0f))), m_Mesh(mesh) {
		using namespace engine;
		s_Instance = this;
		World::RenderData& renderData = *m_World.AddRenderData(*m_Creature, engine::Mat4(1), m_Mesh.GetMeshData());
		renderData.m_MeshData = m_Mesh.GetMeshData();
		renderData.m_Transform = Mat4(1);
	}
};

*/

class UIElement : public engine::UI::Entity {
public:

	engine::UI& m_UI;
	engine::UI::StaticText m_Text;

	engine::Vec2 m_Position;
	float rotation = 0.0f;
	float rotationSpeed = engine::pi / 2;
	float yOffset = 0.0f;
	float targetYOffset = 40.0f;
	int addSign = 1;
	float timer = 0.0f;

	UIElement(engine::UI& UI, const engine::GlyphAtlas& atlas) : m_UI(UI), m_Text(m_UI) {
		using namespace engine;
		m_Text.Initialize("Hello how's it going", atlas, { 1.0f, 1.0f, 1.0f, 1.0f }, 
			TextRenderer::CalcTextSize("Hello how's it going", atlas, { 5, 5 }), engine::TextAlignment::Middle);
	}

	void UILoop(engine::UI& UI) {
		using namespace engine;
		float tempYOffset = Lerp(yOffset, targetYOffset, 0.25f * Time::DeltaTime());
		float deltaYOffset = tempYOffset - yOffset;
		int yOffsetSign = deltaYOffset > 0.0f ? 1 : -1;
		deltaYOffset = Clamp(abs(deltaYOffset), 0.0f, 20.0f * Time::DeltaTime());
		yOffset += deltaYOffset * yOffsetSign;
		if (abs(targetYOffset - yOffset) < 10) {
			targetYOffset *= -1;
		}
		m_Position = Vec2::Lerp(m_Position, m_UI.GetCursorPosition(), 5.0f * Time::DeltaTime());
		m_Text.m_Position = m_Position + (Vec2::Up() * yOffset).Rotated(rotation);
		UI.AddRenderData(m_Text);
		rotation += rotationSpeed * Time::DeltaTime();
		float rand = RandomFloat(0.0f, pi / 8);
		if (timer > 2.0f) {
			timer = 0.0f;
			addSign *= -1;
		}
		rotationSpeed += rand * addSign * Time::DeltaTime();
		timer += Time::DeltaTime();
	}

	void Terminate() {
		m_Text.Terminate();
	}
};

class InputText : public engine::UI::Entity {
public:

	using StaticText = engine::UI::StaticText;
	using DynamicText = engine::UI::DynamicText;
	using Character = engine::TextRenderer::Character;

	engine::UI& m_UI;
	char m_Buffer[64] {};
	const engine::GlyphAtlas& m_GlyphAtlas;
	StaticText* m_Texts;
	size_t m_CharacterCount = 0;
	engine::FontAtlas& m_FontAtlas;
	DynamicText m_DynText;

	uint32_t m_TextLength = 0;

	size_t m_CurrentOffsetIndex = 0;
	engine::Vec2 m_CurrentOffset = 0;
	float m_Timer = 0.0f;

	float sinNum = 0.0f;

	InputText(engine::UI& UI, const engine::GlyphAtlas& atlas, engine::FontAtlas& fontAtlas)
		: m_UI(UI), m_Texts((StaticText*)(malloc(64 * sizeof(StaticText)))), 
			m_GlyphAtlas(atlas), m_FontAtlas(fontAtlas), m_DynText(m_UI, m_FontAtlas) {
		m_DynText.Initialize(VK_NULL_HANDLE);
		m_DynText
			.PutChar('H')
			.PutChar('e')
			.PutChar('l')
			.PutChar('l')
			.PutChar('o');
	}

	void PutChar(unsigned char c) {
		using namespace engine;
		m_DynText.PutChar(c);
	}

	bool PopChar() {
		using namespace engine;
		if (m_CharacterCount == 0) {
			return false;
		}
		m_Texts[m_CharacterCount - 1].~StaticText();
		m_Buffer[m_CharacterCount] = '\0';
		--m_CharacterCount;
	}

	void UILoop(engine::UI& UI) {
		using namespace engine;
		sinNum = fmod(sinNum + 10.0f * Time::DeltaTime(), 2 * pi);
		for (DynamicText::Character& character : m_DynText) {
			character.m_Offset = IntVec2(0, 10 * (sin((float)character.GetLocalPositionX() / 10 + sinNum - pi / 2) + 1));
		}
		for (unsigned int c : Input::GetTextInput()) {
			if (c >= 128) {
				continue;
			}
			else {
				PutChar(c);
			}
		}
		m_DynText.m_Position = UI.GetCursorPosition();
		UI.AddRenderData(m_DynText);
		return;
		IntVec2 pos = UI.GetCursorPosition() - IntVec2(50, 50);
		int startPosX = pos.x;
		for (size_t i = 0; i < m_CharacterCount; i++) {
			StaticText& text = m_Texts[i];
			const Character& character = m_GlyphAtlas.m_Characters[m_Buffer[i]];
			if (character.m_Size.x == 0) {
				pos.x += character.m_Escapement.x;
				continue;
			}
			pos.x += character.m_Size.x / 2;
			IntVec2 offset(0.0f, 10 * (sin((float)(pos.x - startPosX) / 10 + sinNum - pi / 2) + 1));
			/*
			if (m_CurrentOffsetIndex == i) {
				offset = m_CurrentOffset;
			}
			*/
			text.m_Position = pos + IntVec2(0, character.m_Size.y / 2) + IntVec2(character.m_Bearing.x, -character.m_Bearing.y) - offset;
			if (!text.IsNull()) {
				UI.AddRenderData(text);
			}
			pos.x += character.m_Escapement.x / 2;
		}
		m_Timer += Time::DeltaTime();
		/*
		if (m_Timer < 0.5f) {
			m_CurrentOffset.y += 10.0f * Time::DeltaTime();
		}
		else if (m_Timer < 1.0f) {
			m_CurrentOffset.y -= 10.0f * Time::DeltaTime();
		}
		else if (m_CharacterCount) {
			m_Timer = 0;
			m_CurrentOffsetIndex = (m_CurrentOffsetIndex + 1) % m_CharacterCount;
		}
		m_CurrentOffset.y = Clamp(m_CurrentOffset.y, 0.0f, 5.0f);
		*/
	}

	void Terminate() {
		auto end = m_Texts + m_CharacterCount;
		for (auto iter = m_Texts; iter != end; iter++) {
			iter->Terminate();
		}
		free(m_Texts);
		m_DynText.Terminate();
	}
};

int main() {
	using namespace engine;

	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	//glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	GLFWwindow* pWindow = glfwCreateWindow(540, 540, "Test", nullptr, nullptr);

	engine::Engine engine(engine::EngineMode_Play, "Test", pWindow, 1000);

	TextRenderer& textRenderer = engine.GetTextRenderer();

	GlyphAtlas atlas{};
	textRenderer.CreateGlyphAtlas("resources\\fonts\\arial_mt.ttf", 40, atlas);
	const char* text = "Hello, how\nis it going? AVAVAVA";
	TextRenderer::RenderTextInfo renderInfo {
		.m_GlyphAtlas = atlas,
		.m_Spacing = { 2, 2 },
		.m_TextColor = PackColorRBGA({ 1.0f, 1.0f, 1.0f, 0.8f }),
		.m_BackGroundColor = PackColorRBGA({ 0.0f, 0.0f, 0.0f, 0.0f }),
	};

	FontAtlas fontAtlas(engine);

	assert(fontAtlas.LoadFont("resources\\fonts\\arial_mt.ttf", 40));

	TextImage textImage 
			= textRenderer.RenderText<TextAlignment::Middle>(text, renderInfo, { 540, 540 });
	StaticTexture texture(engine);

	texture.Create(VK_FORMAT_R8G8B8A8_SRGB, textImage.m_Extent, textImage.m_Image);

	VkImageView textureImageView = texture.CreateImageView();
	UI& UI = engine.GetUI();
	static VkDescriptorSet testUIDescriptorSet;
	VkDescriptorPool testUIDescriptorPool;
	UI.CreateTexture2DArray<1>(&textureImageView, testUIDescriptorSet, testUIDescriptorPool);
	/*
	UI::Window* uiWindow = UI.AddWindow("Moi", UI::WindowState::Focused, { 0, 0 }, { 540, 540 });
	uiWindow->m_Pipeline2DRenderCallback = [](const UI::Window& window, VkDescriptorSet& outDescriptorSet, uint32_t& outTextureIndex) -> bool {
		outDescriptorSet = testUIDescriptorSet;
		outTextureIndex = 0;
		return true;
	};
	*/

	UIElement element(UI, atlas);
	InputText inputText(UI, atlas, fontAtlas);
	/*
	inputText.PutChar('H');
	inputText.PutChar('e');
	inputText.PutChar('l');
	inputText.PutChar('l');
	inputText.PutChar('o');
	inputText.PutChar('!');
	*/

	UI.AddEntity(&element);
	UI.AddEntity(&inputText);

	Engine::GroundInfo groundInfo {
		.m_TopViewBoundingRect {
			.m_Min { -8.0f, -8.0f },
			.m_Max { 8.0f, 8.0f },
		}
	};

	Engine::ObstacleInfo obstacleInfo {
		.m_Position = {},
		.m_Rotation = Quaternion::AxisRotation(Vec3(0.0f, 1.0f, 0.0), pi / 4),
		.m_Dimensions = { 2.0f, 2.0f, 2.0f },
		.m_ColliderInfo {
			.m_LocalPosition = {},
			.m_Type = Collider::Type::Fence,
			.u_TypeInfo {
				.m_FenceInfo { .m_Dimensions { 4, 4, 4 }, .m_YRotation = pi / 4 },
			}
		},
	};

	World& world = engine.LoadWorld({ 128, 128 }, { 8, 8 }, 1, &groundInfo, 1, &obstacleInfo);

	uint8_t* brickWallImage;
	Vec2_T<uint32_t> brickWallExtent;
	assert(engine.LoadImage("resources\\textures\\brick_wall\\albedo.png", 4, brickWallImage, brickWallExtent));
	StaticTexture brickWallTexture(engine);
	assert(brickWallTexture.Create(VK_FORMAT_R8G8B8A8_SRGB, brickWallExtent, brickWallImage));
	engine.DestroyImage(brickWallImage);
	World::TextureMap textureMap{};
	assert(world.CreateTextureMap(brickWallTexture, textureMap));

	const Engine::DynamicArray<Engine::Ground>& grounds = world.GetGrounds();

	MeshData groundMesh = engine.GetQuadMesh().GetMeshData();

	Mat4 groundTransform = Quaternion::AxisRotation(Vec3(1.0f, 0.0f, 0.0f), -pi / 2).AsMat4();
	groundTransform[0] *= 10.0f;
	groundTransform[1] *= 10.0f;
	groundTransform[2] *= 10.0f;
	groundTransform[3] = { 0.0f, 1.0f, 0.0f, 1.0f };

	auto groundRenderData = world.AddRenderData(grounds[0], groundTransform, groundMesh);
	(*groundRenderData).m_AlbedoTextureDescriptorSet = textureMap.m_DescriptorSet;

	Engine::Obj cubeObj{};
	FILE* fileStream = fopen("resources\\meshes\\cube.obj", "r");
	assert(cubeObj.Load(fileStream));
	fclose(fileStream);

	Engine::DynamicArray<uint32_t> cubeIndices{};
	Engine::DynamicArray<Engine::Vertex> cubeVertices{};

	assert(cubeObj.GetMesh(Engine::Vertex::SetPosition, Engine::Vertex::SetUV, 
		Engine::Vertex::SetNormal, cubeVertices, cubeIndices));	

	StaticMesh cubeMesh(engine);
	cubeMesh.CreateBuffers(cubeVertices.m_Size, cubeVertices.m_Data, cubeIndices.m_Size, cubeIndices.m_Data);

	const Engine::DynamicArray<Engine::Obstacle>& obstacles = world.GetObstacles();

	Mat4 obstacleTransform = obstacleInfo.m_Rotation.AsMat4();
	for (size_t i = 0; i < 3; i++) {
		obstacleTransform[i] *= 2;
	}

	auto obstacleRenderData = world.AddRenderData(obstacles[0], obstacleTransform, cubeMesh.GetMeshData());
	(*obstacleRenderData).m_AlbedoTextureDescriptorSet = textureMap.m_DescriptorSet;
	//world.AddDebugRenderData(obstacles[0], obstacleTransform, Vec4(0.0f, 0.8f, 0.3f, 1.0f), cubeMesh.GetMeshData());

	Player player(world, cubeMesh);
	(*player.m_RenderData).m_AlbedoTextureDescriptorSet = textureMap.m_DescriptorSet;
	//NPC npc(world, playerMesh);

	while (engine.Loop()) {
		float deltaTime = Time::DeltaTime();
		//uiWindow->SetPosition(UI.GetCursorPosition());
	}
	Renderer& renderer = engine.GetRenderer();
	vkDeviceWaitIdle(renderer.m_VulkanDevice);	
	element.Terminate();
	inputText.Terminate();
	world.DestroyTextureMap(textureMap);
	brickWallTexture.Terminate();
	player.m_Mesh.Terminate();
	renderer.DestroyDescriptorPool(testUIDescriptorPool);
	glfwTerminate();
	fontAtlas.Terminate();
	textRenderer.DestroyGlyphAtlas(atlas);
	textRenderer.DestroyTextImage(textImage);
	texture.Terminate();
	renderer.DestroyImageView(textureImageView);
	return 0;
}
