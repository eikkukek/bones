#include "engine.hpp"
#include "random.hpp"
#include <chrono>

class Player : public engine::World::Entity {
public:

	engine::World& m_World;
	engine::StaticMesh& m_Mesh;
	engine::Quaternion m_Rotation;
	engine::PersistentReference<engine::Body> m_Body;
	engine::PersistentReference<engine::WorldRenderData> m_RenderData;

	Player(engine::World& world, engine::StaticMesh& mesh) 
		: m_World(world), 
			m_Body(world.AddBody({ 3.0f, 0.0f, 0.0f }, 2.0f,
				{ 
					.m_LocalPosition {}, 
					.m_Type { engine::Collider::Type::Pole },
					.u_TypeInfo { .m_PoleInfo { .m_Radius = 1, .m_Height = 2 } },
				}
			)), 
		m_Mesh(mesh), m_RenderData(world.AddRenderData(engine::WorldRenderDataFlag_NoSave, *m_Body, engine::Mat4(1), mesh.GetMeshData())) {
		using namespace engine;
	}

	void Update(engine::World& world) {
		using namespace engine;
		using Key = Input::Key;
		Vec3 movementVector = Vec3(
			Input::ReadKeyValue(Key::D) - Input::ReadKeyValue(Key::A),
			0.0f,
			Input::ReadKeyValue(Key::W) - Input::ReadKeyValue(Key::S)
		) * (5 * Time::DeltaTime());
		Body& body = *m_Body;
		if (movementVector != Vec3(0.0f)) {
			body.Move(body.GetPosition() + movementVector);
			world.SetGameCameraView(Mat4::LookAt(Vec3(0.0f, 10.0f, -5.0f), Vec3::Up(), body.GetPosition()));
		}
	}

	void Terminate() {
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

	Engine engine(engine::EngineMode_Editor, "Test", pWindow, 1000);
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

	uint8_t* brickWallImage;
	Vec2_T<uint32_t> brickWallExtent;
	assert(LoadImage("resources\\textures\\brick_wall\\albedo.png", 4, brickWallImage, brickWallExtent));
	StaticTexture brickWallTexture(renderer);
	assert(brickWallTexture.Create(VK_FORMAT_R8G8B8A8_SRGB, brickWallExtent, brickWallImage));
	engine::FreeImage(brickWallImage);

	World& world = engine.GetWorld();

	World::TextureMap textureMap{};
	assert(world.CreateTextureMap(brickWallTexture, textureMap));

	PersistentReference<Area> area = world.AddArea(AreaFlag_NoSave);

	StaticMesh cubeMesh(renderer);
	Array<Vertex, Engine::GetBoxVertexCount()> cubeVertices;
	Array<uint32_t, Engine::GetBoxIndexCount()> cubeIndices;
	Engine::GetBoxMesh(cubeVertices, cubeIndices);
	cubeMesh.CreateBuffers(cubeVertices.Size(), cubeVertices.Data(), cubeIndices.Size(), cubeIndices.Data());

	Engine::GetQuadMesh(quadVertices, quadIndices);

	LogicMesh logicQuadMesh(quadVertices, quadIndices);

	auto obstacle = (*area).AddObstacle(
		"Obstacle",
		{
			.m_Position { 0, 0, 0 },
			.m_YRotation = pi / 4,
			.m_ColliderInfo {
				.m_LocalPosition = {},
				.m_Type = Collider::Type::Fence,
				.u_TypeInfo {
					.m_FenceInfo {
						.m_Dimensions { 2, 2, 2 },
						.m_YRotation = 0,
					}
				}
			}
		}
	);

	auto obstacleRenderData = world.AddRenderData(WorldRenderDataFlag_NoSave, *obstacle, Mat4(1), cubeMesh.GetMeshData());
	(*obstacleRenderData).m_AlbedoTextureDescriptorSet = textureMap.m_DescriptorSet;

	Mat4 groundTransform = Quaternion::AxisRotation(Vec3(1.0f, 0.0f, 0.0f), -pi / 2).AsMat4();

	groundTransform[0] *= 10;
	groundTransform[1] *= 10;
	groundTransform[2] *= 10;
	groundTransform[3].y = -1.0f;

	auto ground = (*area).AddRayTarget(
		{
			.m_LogicMesh = logicQuadMesh,
			.m_Transform = groundTransform,
		}
	);

	MeshData groundMesh = engine.GetQuadMesh().GetMeshData();

	auto groundRenderData = world.AddRenderData(WorldRenderDataFlag_NoSave, *ground, groundTransform, engine.GetQuadMesh().GetMeshData());
	(*groundRenderData).m_AlbedoTextureDescriptorSet = textureMap.m_DescriptorSet;

	Obj cubeObj{};
	FILE* fileStream = fopen("resources\\meshes\\cube.obj", "r");
	assert(cubeObj.Load(fileStream));
	fclose(fileStream);

	/*
	Player player(world, cubeMesh);
	(*player.m_RenderData).m_AlbedoTextureDescriptorSet = textureMap.m_DescriptorSet;

	world.AddEntity(&player);
	*/

	Editor& editor = engine.GetEditor();

	editor.SetInspectedArea(*area);

	while (engine.Loop()) {
		float deltaTime = Time::DeltaTime();
		//uiWindow->SetPosition(UI.GetCursorPosition());
	}
	vkDeviceWaitIdle(renderer.m_VulkanDevice);	
	element.Terminate();
	inputText.Terminate();
	world.DestroyTextureMap(textureMap);
	brickWallTexture.Terminate();
	//player.m_Mesh.Terminate();
	glfwTerminate();
	fontAtlas.Terminate();
	textRenderer.DestroyGlyphAtlas(atlas);
	return 0;
}
