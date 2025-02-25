#include "engine.hpp"
#include "random.hpp"

class Player : public engine::World::Entity {
public:

	engine::World& m_World;
	engine::StaticMesh& m_Mesh;
	engine::Quaternion m_Rotation;
	engine::ObjectID m_BodyID;
	engine::RenderID m_RenderData;

	/*

	Player(engine::World& world, engine::StaticMesh& mesh) 
		: m_World(world), 
			m_BodyID(world.AddBody({ 3.0f, 0.0f, 0.0f }, 2.0f,
				{ 
					.m_LocalPosition {}, 
					.m_Type = engine::Collider::Type::Pole,
					.u_TypeInfo { .m_PoleInfo { .m_Radius = 1, .m_Height = 2 } },
				}
			)),
			m_Mesh(mesh), m_RenderData(engine::Invalid_ID) {
		using namespace engine;
		Body* body = m_World.GetBody(m_BodyID);
		assert(body);
		m_RenderData = world.AddRenderData(engine::WorldRenderDataFlag_NoSave, *body, engine::Mat4(1), mesh.GetMeshData());
	}

	void Update(engine::World& world) {
		using namespace engine;
		using Key = Input::Key;
		Vec3 movementVector = Vec3(
			Input::ReadKeyValue(Key::D) - Input::ReadKeyValue(Key::A),
			0.0f,
			Input::ReadKeyValue(Key::W) - Input::ReadKeyValue(Key::S)
		) * (5 * Time::DeltaTime());
		Body* body = m_World.GetBody(m_BodyID);
		assert(body);
		if (movementVector != Vec3(0.0f)) {
			body->Move(body->GetPosition() + movementVector);
			world.SetGameCameraView(Mat4::LookAt(Vec3(0.0f, 10.0f, -5.0f), Vec3::Up(), body->GetPosition()));
		}
	}

	void Terminate() {
	}
	*/
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

	Obj torus{};
	FILE* torusFs = fopen("resources\\meshes\\torus.obj", "r");
	if (!torusFs) {
		CriticalError(ErrorOrigin::Engine,
			"failed to open torus obj file!");
	}
	bool torusLoaded = torus.Load(torusFs);
	fclose(torusFs);
	if (!torusLoaded) {
		CriticalError(ErrorOrigin::Engine,
			"failed to load torus obj file!");
	}
	Engine::SetTorusObj(torus);

	GLFWwindow* pWindow = glfwCreateWindow(540, 540, "Test", nullptr, nullptr);
	Engine engine(EngineState_Editor | EngineState_EditorView, "Test", pWindow, 1000);

	Renderer& renderer = engine.GetRenderer();
	TextRenderer textRenderer = engine.GetTextRenderer();

	GlyphAtlas atlas {};
	if (!textRenderer.CreateGlyphAtlas("resources\\fonts\\arial_mt.ttf", 40, atlas)) {
		LogError("failed to create glyph atlas!");
	}

	FontAtlas fontAtlas(renderer, textRenderer);
	if (!fontAtlas.LoadFont("resources\\fonts\\arial_mt.ttf", 40)) {
		LogError("failed to load font!");
	}

	UI& UI = engine.GetUI();

	UIElement element(UI, atlas);
	InputText inputText(UI, atlas, fontAtlas);

	UI.AddEntity(&element);
	UI.AddEntity(&inputText);

	uint8_t* brickWallImage;
	Vec2_T<uint32_t> brickWallExtent;
	if (!LoadImage("resources\\textures\\brick_wall\\albedo.png", 4, brickWallImage, brickWallExtent)) {
		LogError("failed to load brick wall texture!");
	}
	StaticTexture brickWallTexture(renderer);
	if (!brickWallTexture.Create(VK_FORMAT_R8G8B8A8_SRGB, brickWallExtent, brickWallImage)) {
		LogError("failed to create brick wall texture!");
	}
	engine::FreeImage(brickWallImage);

	World& world = engine.GetWorld();

	World::TextureMap textureMap{};
	if (!world.CreateTextureMap(brickWallTexture, textureMap)) {
		LogError("failed to create texture map!");
	}

	ObjectID areaID = world.AddArea(AreaFlag_NoSave);
	Area* pArea = world.GetArea(areaID);
	Area& area = *pArea;

	Obj sphere{};
	FILE* fs = fopen("resources\\meshes\\sphere.obj", "r");
	sphere.Load(fs);
	fclose(fs);
	DynamicArray<Vertex> vertices{};
	DynamicArray<uint32_t> indices{};
	sphere.GetMesh(Vertex::SetPosition, Vertex::SetUV, Vertex::SetNormal, vertices, indices);
	StaticMesh sphereMesh(renderer);
	sphereMesh.CreateBuffers(vertices.Size(), vertices.Data(), indices.Size(), indices.Data());

	Array<Vertex, 4> quadVertices;
	Array<uint32_t, 6> quadIndices;

	LogicMesh logicQuadMesh(quadVertices, quadIndices);

	ObjectID obstacleID = area.AddBody(
		"Obstacle",
		Vec3(3.0f, 0.0f, 0.0f),
		Quaternion::Identity(),
		PhysicsLayer::Moving,
		{
			.m_ColliderShape = Body::ColliderShape::Sphere,
			.u_ShapeCreateInfo {
				.m_Sphere { 
					.m_Radius = 1.0f,
				},
			},
		},
		nullptr
	);

	Body* obstacle = area.GetBody(obstacleID);

	//obstacle->AddForce(Vec3(10000.0f, 0.0f, 0.0f));
	obstacle->Move(Vec3(0.0f, 0.0f, 3.0f), Quaternion::Identity(), 5.0f);

	RenderID obstacleRenderID = world.AddRenderData(WorldRenderDataFlag_NoSave, *obstacle, Mat4(1), sphereMesh.GetMeshData());
	world.GetRenderData(obstacleRenderID)->m_AlbedoTextureDescriptorSet = textureMap.m_DescriptorSet;

	ObjectID groundID = area.AddBody(
		"Ground",
		Vec3(0.0f, -5.0f, 0.0f),
		Quaternion::Identity(),
		PhysicsLayer::NonMoving,
		{
			.m_ColliderShape = Body::ColliderShape::Box,
			.u_ShapeCreateInfo {
				.m_Box {
					.m_HalfExtent = Vec3(50.0f, 1.0f, 50.0f),
					.m_ConvexRadius = 0.05f,
				},
			},
		},
		nullptr
	);

	Body* ground = area.GetBody(groundID);

	Mat4 groundTransform = Quaternion::AxisRotation(Vec3(1.0f, 0.0f, 0.0f), -pi / 2).AsMat4();

	groundTransform[0] *= 50;
	groundTransform[1] *= 50;
	groundTransform[2] *= 1;
	groundTransform[3].y = 0.0f;

	RenderID groundRenderID = world.AddRenderData(WorldRenderDataFlag_NoSave, *ground, groundTransform, engine.GetBoxMesh().GetMeshData());
	world.GetRenderData(groundRenderID)->m_AlbedoTextureDescriptorSet = textureMap.m_DescriptorSet;

	Obj cubeObj{};
	FILE* fileStream = fopen("resources\\meshes\\cube.obj", "r");
	cubeObj.Load(fileStream);
	fclose(fileStream);

	/*
	Player player(world, cubeMesh);
	(*player.m_RenderData).m_AlbedoTextureDescriptorSet = textureMap.m_DescriptorSet;

	world.AddEntity(&player);
	*/

	Editor& editor = engine.GetEditor();

	editor.SetInspectedArea(areaID);

	while (engine.Loop()) {}

	vkDeviceWaitIdle(renderer.m_VulkanDevice);	
	sphereMesh.Terminate();
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
