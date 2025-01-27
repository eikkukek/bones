#include "engine.hpp"
#include "random.hpp"
#include <chrono>

class Player : public engine::World::Entity {
public:

	static inline Player* s_Instance = nullptr;
	
	engine::World& m_World;
	engine::StaticMesh& m_Mesh;
	engine::Quaternion m_Rotation;
	engine::PersistentReference<engine::Body> m_Body;
	engine::PersistentReference<engine::World::RenderData> m_RenderData;

	/*
	static engine::Vec3 MovementVectorUpdate(const engine::Body& creature) {
		using namespace engine;
		using Key = Input::Key;
		return Vec3(
				Input::ReadKeyValue(Key::D) - Input::ReadKeyValue(Key::A),
				0.0f,
				Input::ReadKeyValue(Key::W) - Input::ReadKeyValue(Key::S)) * (5 * Time::DeltaTime());
	}

	static void MoveCallback(const engine::Creature& creature, const engine::Vec3& position, const engine::Vec3& deltaPositio) {
		using namespace engine;
		s_Instance->m_RenderData.m_Val->m_Transform[3] = Vec4(position, 1.0f);
	}

	static void CameraFollowCallback(const engine::Creature& creature, engine::Vec3& outCameraPosition, engine::Vec3& outCameraLookAt,
		uint32_t directionalLightCount, engine::UnidirectionalLight directionalLights[]) {
		using namespace engine;
		const engine::Vec3& creaturePos = creature.GetPosition();
		static const Vec3 cameraOffset(20.0f, 20.0f, -20.0f);
		static const Vec3 lookAtOffset(0.0f, 0.0f, 10.0f);
		static const Vec3 lightOffset(50.0f, 50.0f, 2.0f);
		outCameraPosition = creaturePos + cameraOffset;
		outCameraLookAt = creaturePos + lookAtOffset;
		directionalLights[0].SetViewMatrix(Mat4::LookAt(creaturePos + lightOffset, Vec3::Up(), creaturePos));
	}
	*/

	Player(engine::World& world, engine::StaticMesh& mesh) 
		: m_World(world), 
			m_Body(world.AddBody({ 3.0f, 0.0f, 0.0f }, 2.0f,
				{ 
					.m_LocalPosition {}, 
					.m_Type { engine::Collider::Type::Pole },
					.u_TypeInfo { .m_PoleInfo { .m_Radius = 1, .m_Height = 2 } },
				}
			)), 
			m_Mesh(mesh), m_RenderData(world.AddRenderData(*m_Body, {}, {})) {
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

	using DynamicText = engine::UI::DynamicText;
	using Character = engine::TextRenderer::Character;

	engine::UI& m_UI;
	char m_Buffer[64] {};
	size_t m_CharacterCount = 0;
	engine::FontAtlas& m_FontAtlas;
	DynamicText m_DynText;

	uint32_t m_TextLength = 0;

	size_t m_CurrentOffsetIndex = 0;
	engine::Vec2 m_CurrentOffset = 0;
	float m_Timer = 0.0f;

	float sinNum = 0.0f;

	InputText(engine::UI& UI, const engine::GlyphAtlas& atlas, engine::FontAtlas& fontAtlas)
		: m_UI(UI), m_FontAtlas(fontAtlas), m_DynText(m_UI, m_FontAtlas) {
		m_DynText.Initialize(VK_NULL_HANDLE);
		m_DynText
			.PutChar('H')
			.PutChar('e')
			.PutChar('l')
			.PutChar('l')
			.PutChar('o');
	}

	void UILoop(engine::UI& UI) {
		using namespace engine;
		sinNum = fmod(sinNum + 10.0f * Time::DeltaTime(), 2 * pi);
		for (DynamicText::Character& character : m_DynText) {
			character.m_Offset = IntVec2(0, 10 * (sin((float)character.GetLocalPositionX() / 10 + sinNum - pi / 2) + 1));
		}
		/*
		for (unsigned int c : Input::GetTextInput()) {
			if (c >= 128) {
				continue;
			}
			else {
				m_DynText.PutChar(c);
			}
		}
		*/
		m_DynText.m_Position = UI.GetCursorPosition();
		UI.AddRenderData(m_DynText);
	}

	void Terminate() {
		m_DynText.Terminate();
	}
};

int main() {
	using namespace engine;

	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	GLFWwindow* pWindow = glfwCreateWindow(540, 540, "Test", nullptr, nullptr);

	Engine engine(engine::EngineMode_Play, "Test", pWindow, 1000);	
	Renderer& renderer = engine.GetRenderer();
	TextRenderer textRenderer = engine.GetTextRenderer();

	GlyphAtlas atlas{};
	textRenderer.CreateGlyphAtlas("resources\\fonts\\arial_mt.ttf", 40, atlas);

	FontAtlas fontAtlas(renderer, textRenderer);
	assert(fontAtlas.LoadFont("resources\\fonts\\arial_mt.ttf", 40));

	UI& UI = engine.GetUI();

	UIElement element(UI, atlas);
	InputText inputText(UI, atlas, fontAtlas);

	UI.AddEntity(&element);
	UI.AddEntity(&inputText);

	Array<Vertex, 4> quadVertices;
	Array<uint32_t, 6> quadIndices;

	Engine::GetQuadMesh(quadVertices, quadIndices);

	LogicMesh logicQuadMesh(quadVertices, quadIndices);

	Mat4 groundTransform = Quaternion::AxisRotation(Vec3(1.0f, 0.0f, 0.0f), -pi / 4).AsMat4();
	groundTransform[0] *= 100.0f;
	groundTransform[1] *= 100.0f;
	groundTransform[2] *= 100.0f;
	groundTransform[3] = { 0.0f, -1.0f, 0.0f, 1.0f };
	
	World::Ground::CreateInfo groundInfo {
		.m_LogicMesh = logicQuadMesh,
		.m_Transform = groundTransform,
	};

	World::Obstacle::CreateInfo obstacleInfo {
		.m_Position = {},
		.m_YRotation = pi / 4,
		.m_ColliderInfo {
			.m_LocalPosition = {},
			.m_Type = Collider::Type::Fence,
			.u_TypeInfo {
				.m_FenceInfo { .m_Dimensions { 4, 4, 4 }, .m_YRotation = 0 },
			}
		},
	};

	World& world = engine.LoadWorld({ 128, 128 }, { 8, 8 }, 1, &groundInfo, 1, &obstacleInfo);

	uint8_t* brickWallImage;
	Vec2_T<uint32_t> brickWallExtent;
	assert(LoadImage("resources\\textures\\brick_wall\\albedo.png", 4, brickWallImage, brickWallExtent));
	StaticTexture brickWallTexture(renderer);
	assert(brickWallTexture.Create(VK_FORMAT_R8G8B8A8_SRGB, brickWallExtent, brickWallImage));
	FreeImage(brickWallImage);
	World::TextureMap textureMap{};
	assert(world.CreateTextureMap(brickWallTexture, textureMap));

	const DynamicArray<World::Ground>& grounds = world.GetGrounds();

	MeshData groundMesh = engine.GetQuadMesh().GetMeshData();

	auto groundRenderData = world.AddRenderData(grounds[0], groundTransform, groundMesh);
	(*groundRenderData).m_AlbedoTextureDescriptorSet = textureMap.m_DescriptorSet;

	Obj cubeObj{};
	FILE* fileStream = fopen("resources\\meshes\\cube.obj", "r");
	assert(cubeObj.Load(fileStream));
	fclose(fileStream);

	DynamicArray<uint32_t> cubeIndices{};
	DynamicArray<Vertex> cubeVertices{};

	assert(cubeObj.GetMesh(Vertex::SetPosition, Vertex::SetUV, 
		Vertex::SetNormal, cubeVertices, cubeIndices));	

	StaticMesh cubeMesh(renderer);
	cubeMesh.CreateBuffers(cubeVertices.Size(), cubeVertices.Data(), cubeIndices.Size(), cubeIndices.Data());

	const DynamicArray<World::Obstacle>& obstacles = world.GetObstacles();

	Mat4 obstacleTransform = Quaternion::AxisRotation(Vec3::Up(), obstacleInfo.m_YRotation).AsMat4();
	for (size_t i = 0; i < 3; i++) {
		obstacleTransform[i] *= 2;
	}

	auto obstacleRenderData = world.AddRenderData(obstacles[0], obstacleTransform, cubeMesh.GetMeshData());
	(*obstacleRenderData).m_AlbedoTextureDescriptorSet = textureMap.m_DescriptorSet;
	//world.AddDebugRenderData(obstacles[0], obstacleTransform, Vec4(0.0f, 0.8f, 0.3f, 1.0f), cubeMesh.GetMeshData());

	auto obstacleRenderData2 = world.AddRenderData(obstacles[0], groundTransform, engine.GetQuadMesh().GetMeshData());
	(*obstacleRenderData2).m_AlbedoTextureDescriptorSet = textureMap.m_DescriptorSet;

	Player player(world, cubeMesh);
	(*player.m_RenderData).m_AlbedoTextureDescriptorSet = textureMap.m_DescriptorSet;
	//NPC npc(world, playerMesh);

	while (engine.Loop()) {
		float deltaTime = Time::DeltaTime();
		//uiWindow->SetPosition(UI.GetCursorPosition());
	}
	vkDeviceWaitIdle(renderer.m_VulkanDevice);	
	element.Terminate();
	inputText.Terminate();
	world.DestroyTextureMap(textureMap);
	brickWallTexture.Terminate();
	player.m_Mesh.Terminate();
	glfwTerminate();
	fontAtlas.Terminate();
	textRenderer.DestroyGlyphAtlas(atlas);
	return 0;
}
