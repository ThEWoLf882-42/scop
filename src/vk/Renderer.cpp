#include "scop/vk/Renderer.hpp"
#include "scop/vk/Uploader.hpp"
#include "scop/io/ObjLoader.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "scop/math/Mat4.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iostream>
#include <limits>
#include <sstream>
#include <vector>

namespace
{

	struct alignas(16) UBOData
	{
		scop::math::Mat4 vp;
		scop::math::Mat4 model;
		float lightDir[4];
		float baseColor[4];
		float cameraPos[4];
		float spec[4];
	};

	static inline float degToRad(float d) { return d * 3.14159265f / 180.0f; }

	static std::string toLower(std::string s)
	{
		for (size_t i = 0; i < s.size(); ++i)
			s[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(s[i])));
		return s;
	}

	static bool endsWithObj(const std::string &path)
	{
		const std::string p = toLower(path);
		if (p.size() < 4)
			return false;
		return p.compare(p.size() - 4, 4, ".obj") == 0;
	}

	static std::string baseName(const std::string &path)
	{
		size_t p = path.find_last_of("/\\");
		if (p == std::string::npos)
			return path;
		return path.substr(p + 1);
	}

	// Support both Vertex.pos being {x,y,z} or [0..2]
	template <typename V>
	static auto px_impl(const V &v, int) -> decltype(v.pos.x, float()) { return v.pos.x; }
	template <typename V>
	static auto px_impl(const V &v, long) -> decltype(v.pos[0], float()) { return v.pos[0]; }

	template <typename V>
	static auto py_impl(const V &v, int) -> decltype(v.pos.y, float()) { return v.pos.y; }
	template <typename V>
	static auto py_impl(const V &v, long) -> decltype(v.pos[1], float()) { return v.pos[1]; }

	template <typename V>
	static auto pz_impl(const V &v, int) -> decltype(v.pos.z, float()) { return v.pos.z; }
	template <typename V>
	static auto pz_impl(const V &v, long) -> decltype(v.pos[2], float()) { return v.pos[2]; }

	template <typename V>
	static float px(const V &v) { return px_impl(v, 0); }
	template <typename V>
	static float py(const V &v) { return py_impl(v, 0); }
	template <typename V>
	static float pz(const V &v) { return pz_impl(v, 0); }

	// Vertex.normal is used as COLOR for lines
	static void pushLine(std::vector<scop::vk::Vertex> &out,
						 float x1, float y1, float z1,
						 float x2, float y2, float z2,
						 float r, float g, float b)
	{
		using scop::vk::Vertex;
		out.push_back(Vertex{{x1, y1, z1}, {r, g, b}});
		out.push_back(Vertex{{x2, y2, z2}, {r, g, b}});
	}

	static std::vector<scop::vk::Vertex> makeGridAxes(int half = 10, float step = 1.0f)
	{
		using scop::vk::Vertex;
		std::vector<Vertex> out;

		const float y = 0.0f;
		const float size = static_cast<float>(half) * step;

		for (int i = -half; i <= half; ++i)
		{
			float p = static_cast<float>(i) * step;
			const bool center = (i == 0);
			const float k = center ? 0.35f : 0.18f;

			pushLine(out, -size, y, p, size, y, p, k, k, k);
			pushLine(out, p, y, -size, p, y, size, k, k, k);
		}

		const float ax = size * 1.15f;
		pushLine(out, 0, 0, 0, ax, 0, 0, 1.f, 0.f, 0.f);
		pushLine(out, 0, 0, 0, 0, ax, 0, 0.f, 1.f, 0.f);
		pushLine(out, 0, 0, 0, 0, 0, ax, 0.f, 0.f, 1.f);

		return out;
	}

} // namespace

namespace scop::vk
{

	Renderer::Renderer(int width, int height, const char *title)
	{
		init(width, height, title, std::string());
	}

	Renderer::Renderer(int width, int height, const char *title, const std::string &initialObjPath)
	{
		init(width, height, title, initialObjPath);
	}

	Renderer::~Renderer() noexcept
	{
		if (ctx_.device() != VK_NULL_HANDLE)
			vkDeviceWaitIdle(ctx_.device());
	}

	void Renderer::rebuildDebugLines()
	{
		std::vector<Vertex> lines = makeGridAxes(10, 1.0f);

		if (showBounds_ && hasModel_)
		{
			const float r = 1.0f, g = 1.0f, b = 0.15f;

			const float minX = aabbMin_[0], minY = aabbMin_[1], minZ = aabbMin_[2];
			const float maxX = aabbMax_[0], maxY = aabbMax_[1], maxZ = aabbMax_[2];

			// 8 corners
			const float x0 = minX, x1 = maxX;
			const float y0 = minY, y1 = maxY;
			const float z0 = minZ, z1 = maxZ;

			// bottom rectangle
			pushLine(lines, x0, y0, z0, x1, y0, z0, r, g, b);
			pushLine(lines, x1, y0, z0, x1, y0, z1, r, g, b);
			pushLine(lines, x1, y0, z1, x0, y0, z1, r, g, b);
			pushLine(lines, x0, y0, z1, x0, y0, z0, r, g, b);

			// top rectangle
			pushLine(lines, x0, y1, z0, x1, y1, z0, r, g, b);
			pushLine(lines, x1, y1, z0, x1, y1, z1, r, g, b);
			pushLine(lines, x1, y1, z1, x0, y1, z1, r, g, b);
			pushLine(lines, x0, y1, z1, x0, y1, z0, r, g, b);

			// vertical edges
			pushLine(lines, x0, y0, z0, x0, y1, z0, r, g, b);
			pushLine(lines, x1, y0, z0, x1, y1, z0, r, g, b);
			pushLine(lines, x1, y0, z1, x1, y1, z1, r, g, b);
			pushLine(lines, x0, y0, z1, x0, y1, z1, r, g, b);
		}

		linesVertexCount_ = static_cast<uint32_t>(lines.size());

		Uploader uploader(ctx_.device(), ctx_.indices().graphicsFamily.value(), ctx_.graphicsQueue());
		linesVB_ = VertexBuffer(ctx_.device(), ctx_.physicalDevice(), uploader, lines);
	}

	bool Renderer::loadModelFromPath(const std::string &path)
	{
		scop::io::MeshData mesh;
		try
		{
			mesh = scop::io::loadObj(path, true);
		}
		catch (const std::exception &e)
		{
			std::cerr << "OBJ load failed: " << e.what() << "\n";
			std::cerr << "No fallback mesh. Provide a valid .obj.\n";
			hasModel_ = false;
			modelVertexCount_ = 0;
			modelIndexCount_ = 0;
			modelLabel_ = "Drop .obj onto window";
			return false;
		}

		if (mesh.vertices.empty() || mesh.indices.empty())
		{
			std::cerr << "OBJ loaded but has no geometry.\n";
			hasModel_ = false;
			modelVertexCount_ = 0;
			modelIndexCount_ = 0;
			modelLabel_ = "Drop .obj onto window";
			return false;
		}

		modelVertexCount_ = static_cast<uint32_t>(mesh.vertices.size());
		modelIndexCount_ = static_cast<uint32_t>(mesh.indices.size());

		// AABB (model space)
		{
			float minX = std::numeric_limits<float>::infinity();
			float minY = std::numeric_limits<float>::infinity();
			float minZ = std::numeric_limits<float>::infinity();
			float maxX = -std::numeric_limits<float>::infinity();
			float maxY = -std::numeric_limits<float>::infinity();
			float maxZ = -std::numeric_limits<float>::infinity();

			for (const auto &v : mesh.vertices)
			{
				minX = std::min(minX, px(v));
				minY = std::min(minY, py(v));
				minZ = std::min(minZ, pz(v));
				maxX = std::max(maxX, px(v));
				maxY = std::max(maxY, py(v));
				maxZ = std::max(maxZ, pz(v));
			}

			aabbMin_[0] = minX;
			aabbMin_[1] = minY;
			aabbMin_[2] = minZ;
			aabbMax_[0] = maxX;
			aabbMax_[1] = maxY;
			aabbMax_[2] = maxZ;

			// Auto-fit based on AABB
			const float cx = (minX + maxX) * 0.5f;
			const float cz = (minZ + maxZ) * 0.5f;

			fitOffsetX_ = -cx;
			fitOffsetY_ = -minY;
			fitOffsetZ_ = -cz;

			const float ex = (maxX - minX);
			const float ey = (maxY - minY);
			const float ez = (maxZ - minZ);
			const float maxE = std::max(ex, std::max(ey, ez));

			const float target = 2.0f;
			fitScale_ = (maxE > 0.000001f) ? (target / maxE) : 1.0f;

			userScale_ = 1.0f;
		}

		vkDeviceWaitIdle(ctx_.device());

		Uploader uploader(ctx_.device(), ctx_.indices().graphicsFamily.value(), ctx_.graphicsQueue());
		VertexBuffer newVB(ctx_.device(), ctx_.physicalDevice(), uploader, mesh.vertices);
		IndexBuffer newIB(ctx_.device(), ctx_.physicalDevice(), uploader, mesh.indices);

		vb_ = std::move(newVB);
		ib_ = std::move(newIB);

		hasModel_ = true;
		modelLabel_ = baseName(path);

		// If bbox is enabled, rebuild the debug lines buffer (grid + bbox)
		if (showBounds_)
			rebuildDebugLines();

		// re-record if rendering is already set up
		if (!fbs_.get().empty() && modelPipe_.pipeline() != VK_NULL_HANDLE)
		{
			cmds_.recordScene(modelPipe_.renderPass(),
							  fbs_.get(),
							  swap_.extent(),
							  modelPipe_.pipeline(),
							  modelPipe_.layout(),
							  desc_.sets(),
							  hasModel_ ? vb_.buffer() : VK_NULL_HANDLE,
							  hasModel_ ? ib_.buffer() : VK_NULL_HANDLE,
							  hasModel_ ? ib_.count() : 0u,
							  VK_INDEX_TYPE_UINT32,
							  linesPipe_.pipeline(),
							  linesVB_.buffer(),
							  linesVertexCount_);
		}

		std::cerr << "Loaded OBJ: " << path
				  << " verts=" << mesh.vertices.size()
				  << " idx=" << mesh.indices.size() << "\n";
		return true;
	}

	void Renderer::init(int width, int height, const char *title)
	{
		init(width, height, title, std::string());
	}

	void Renderer::init(int width, int height, const char *title, const std::string &initialObjPath)
	{
		ctx_.init(width, height, title);

		glfwSetWindowUserPointer(ctx_.window(), this);

		glfwSetFramebufferSizeCallback(
			ctx_.window(),
			[](GLFWwindow *win, int, int)
			{
				auto *self = static_cast<Renderer *>(glfwGetWindowUserPointer(win));
				if (self)
					self->framebufferResized_ = true;
			});

		glfwSetScrollCallback(
			ctx_.window(),
			[](GLFWwindow *win, double, double yoff)
			{
				auto *self = static_cast<Renderer *>(glfwGetWindowUserPointer(win));
				if (!self)
					return;
				self->fovDeg_ -= static_cast<float>(yoff) * 2.0f;
				self->fovDeg_ = std::clamp(self->fovDeg_, 20.0f, 90.0f);
			});

		glfwSetInputMode(ctx_.window(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		cursorLocked_ = true;

		// Auto-unlock on focus loss (helps drag from Finder)
		glfwSetWindowFocusCallback(ctx_.window(), [](GLFWwindow *win, int focused)
								   {
        auto* self = static_cast<Renderer*>(glfwGetWindowUserPointer(win));
        if (!self) return;

        if (focused == GLFW_FALSE) {
            if (self->cursorLocked_) {
                self->cursorLocked_ = false;
                glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                self->firstMouse_ = true;
            }
        } });

		glfwSetCursorPosCallback(
			ctx_.window(),
			[](GLFWwindow *win, double xpos, double ypos)
			{
				auto *self = static_cast<Renderer *>(glfwGetWindowUserPointer(win));
				if (!self || !self->cursorLocked_)
					return;

				if (self->firstMouse_)
				{
					self->firstMouse_ = false;
					self->lastMouseX_ = xpos;
					self->lastMouseY_ = ypos;
					return;
				}

				const double dx = xpos - self->lastMouseX_;
				const double dy = ypos - self->lastMouseY_;
				self->lastMouseX_ = xpos;
				self->lastMouseY_ = ypos;

				const float sens = 0.12f;
				self->yawDeg_ += static_cast<float>(dx) * sens;
				self->pitchDeg_ -= static_cast<float>(dy) * sens;
				self->pitchDeg_ = std::clamp(self->pitchDeg_, -89.0f, 89.0f);
			});

		// Drag & Drop
		glfwSetDropCallback(
			ctx_.window(),
			[](GLFWwindow *win, int count, const char **paths)
			{
				auto *self = static_cast<Renderer *>(glfwGetWindowUserPointer(win));
				if (!self || count <= 0 || !paths)
					return;

				self->droppedObjs_.clear();
				self->droppedIndex_ = 0;

				for (int i = 0; i < count; ++i)
				{
					if (!paths[i])
						continue;
					std::string p = paths[i];
					if (endsWithObj(p))
						self->droppedObjs_.push_back(p);
				}

				if (self->droppedObjs_.empty())
				{
					std::cerr << "Dropped files, but none are .obj\n";
					return;
				}

				self->pendingPath_ = self->droppedObjs_[0];
				self->hasPendingLoad_ = true;

				std::cerr << "Drop playlist (" << self->droppedObjs_.size() << "):\n";
				for (size_t i = 0; i < self->droppedObjs_.size(); ++i)
					std::cerr << "  [" << i << "] " << self->droppedObjs_[i] << "\n";
				std::cerr << "Use [ and ] to cycle.\n";
			});

		// start: no model
		hasModel_ = false;
		modelVertexCount_ = 0;
		modelIndexCount_ = 0;
		modelLabel_ = "Drop .obj onto window";

		// build grid lines buffer now (bbox off by default)
		rebuildDebugLines();

		// If initial OBJ is provided, load it now
		if (!initialObjPath.empty())
		{
			if (endsWithObj(initialObjPath) && loadModelFromPath(initialObjPath))
			{
				droppedObjs_.clear();
				droppedObjs_.push_back(initialObjPath);
				droppedIndex_ = 0;
			}
			else
			{
				std::cerr << "Initial path is not a valid .obj: " << initialObjPath << "\n";
			}
		}

		recreateSwapchain();

		lastTime_ = glfwGetTime();
		fpsAccum_ = 0.0;
		fpsFrames_ = 0;
		modelTime_ = 0.0f;
	}

	void Renderer::recreateSwapchain()
	{
		int w = 0, h = 0;
		glfwGetFramebufferSize(ctx_.window(), &w, &h);
		while (w == 0 || h == 0)
		{
			glfwWaitEvents();
			glfwGetFramebufferSize(ctx_.window(), &w, &h);
		}

		vkDeviceWaitIdle(ctx_.device());

		swap_ = Swapchain(ctx_);
		depth_ = DepthResources(ctx_.device(), ctx_.physicalDevice(), swap_.extent());

		const size_t imageCount = swap_.imageViews().size();

		ubos_.clear();
		ubos_.reserve(imageCount);

		std::vector<VkBuffer> uboBuffers;
		uboBuffers.reserve(imageCount);

		for (size_t i = 0; i < imageCount; ++i)
		{
			ubos_.emplace_back(ctx_.device(), ctx_.physicalDevice(), sizeof(UBOData));
			uboBuffers.push_back(ubos_.back().buffer());
		}

		desc_ = Descriptors(ctx_.device(), uboBuffers, sizeof(UBOData));

		VkPolygonMode mode = VK_POLYGON_MODE_FILL;
		if (wireframe_ && ctx_.wireframeSupported())
			mode = VK_POLYGON_MODE_LINE;

		modelPipe_ = Pipeline(ctx_.device(), swap_.imageFormat(), depth_.format(), swap_.extent(),
							  "shaders/tri.vert.spv", "shaders/tri.frag.spv",
							  desc_.layout(), mode);

		fbs_ = Framebuffers(ctx_.device(), modelPipe_.renderPass(), swap_.imageViews(), depth_.view(), swap_.extent());

		linesPipe_ = PipelineVariant(ctx_.device(),
									 modelPipe_.renderPass(),
									 modelPipe_.layout(),
									 depth_.format(),
									 swap_.extent(),
									 "shaders/lines.vert.spv",
									 "shaders/lines.frag.spv",
									 VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
									 false,
									 VK_CULL_MODE_NONE);

		cmds_ = Commands(ctx_.device(), ctx_.indices().graphicsFamily.value(), fbs_.size());

		cmds_.recordScene(modelPipe_.renderPass(),
						  fbs_.get(),
						  swap_.extent(),
						  modelPipe_.pipeline(),
						  modelPipe_.layout(),
						  desc_.sets(),
						  hasModel_ ? vb_.buffer() : VK_NULL_HANDLE,
						  hasModel_ ? ib_.buffer() : VK_NULL_HANDLE,
						  hasModel_ ? ib_.count() : 0u,
						  VK_INDEX_TYPE_UINT32,
						  linesPipe_.pipeline(),
						  linesVB_.buffer(),
						  linesVertexCount_);

		presenter_ = FramePresenter(ctx_.device(),
									ctx_.graphicsQueue(),
									ctx_.presentQueue(),
									swap_,
									cmds_,
									2);

		framebufferResized_ = false;
	}

	void Renderer::pollEvents() { glfwPollEvents(); }
	bool Renderer::shouldClose() const { return glfwWindowShouldClose(ctx_.window()) != 0; }
	void Renderer::requestClose() { glfwSetWindowShouldClose(ctx_.window(), GLFW_TRUE); }

	void Renderer::draw()
	{
		if (framebufferResized_)
		{
			recreateSwapchain();
			return;
		}

		if (hasPendingLoad_)
		{
			hasPendingLoad_ = false;
			loadModelFromPath(pendingPath_);
		}

		const double now = glfwGetTime();
		const float dt = static_cast<float>(now - lastTime_);
		lastTime_ = now;

		// Cycle playlist with [ and ]
		const bool lbDown = glfwGetKey(ctx_.window(), GLFW_KEY_LEFT_BRACKET) == GLFW_PRESS;
		if (lbDown && !lbWasDown_ && !droppedObjs_.empty())
		{
			droppedIndex_ = (droppedIndex_ == 0) ? (droppedObjs_.size() - 1) : (droppedIndex_ - 1);
			pendingPath_ = droppedObjs_[droppedIndex_];
			hasPendingLoad_ = true;
		}
		lbWasDown_ = lbDown;

		const bool rbDown = glfwGetKey(ctx_.window(), GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS;
		if (rbDown && !rbWasDown_ && !droppedObjs_.empty())
		{
			droppedIndex_ = (droppedIndex_ + 1) % droppedObjs_.size();
			pendingPath_ = droppedObjs_[droppedIndex_];
			hasPendingLoad_ = true;
		}
		rbWasDown_ = rbDown;

		// ESC toggles mouse lock (NOT quit)
		const bool escDown = glfwGetKey(ctx_.window(), GLFW_KEY_ESCAPE) == GLFW_PRESS;
		if (escDown && !escWasDown_)
		{
			cursorLocked_ = !cursorLocked_;
			glfwSetInputMode(ctx_.window(), GLFW_CURSOR,
							 cursorLocked_ ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
			firstMouse_ = true;
		}
		escWasDown_ = escDown;

		// R resets camera
		const bool rDown = glfwGetKey(ctx_.window(), GLFW_KEY_R) == GLFW_PRESS;
		if (rDown && !rWasDown_)
		{
			camX_ = 0.0f;
			camY_ = 0.0f;
			camZ_ = 2.5f;
			yawDeg_ = -90.0f;
			pitchDeg_ = 0.0f;
			fovDeg_ = 55.0f;
			firstMouse_ = true;
		}
		rWasDown_ = rDown;

		// B toggles bounding box
		const bool bDown = glfwGetKey(ctx_.window(), GLFW_KEY_B) == GLFW_PRESS;
		if (bDown && !bWasDown_)
		{
			showBounds_ = !showBounds_;
			vkDeviceWaitIdle(ctx_.device());
			rebuildDebugLines();

			if (!fbs_.get().empty() && modelPipe_.pipeline() != VK_NULL_HANDLE)
			{
				cmds_.recordScene(modelPipe_.renderPass(),
								  fbs_.get(),
								  swap_.extent(),
								  modelPipe_.pipeline(),
								  modelPipe_.layout(),
								  desc_.sets(),
								  hasModel_ ? vb_.buffer() : VK_NULL_HANDLE,
								  hasModel_ ? ib_.buffer() : VK_NULL_HANDLE,
								  hasModel_ ? ib_.count() : 0u,
								  VK_INDEX_TYPE_UINT32,
								  linesPipe_.pipeline(),
								  linesVB_.buffer(),
								  linesVertexCount_);
			}
		}
		bWasDown_ = bDown;

		// SPACE pauses
		const bool spDown = glfwGetKey(ctx_.window(), GLFW_KEY_SPACE) == GLFW_PRESS;
		if (spDown && !spaceWasDown_)
			paused_ = !paused_;
		spaceWasDown_ = spDown;

		// F toggle auto-fit
		const bool fDown = glfwGetKey(ctx_.window(), GLFW_KEY_F) == GLFW_PRESS;
		if (fDown && !fWasDown_)
			autoFit_ = !autoFit_;
		fWasDown_ = fDown;

		// T toggle auto-rotate
		const bool tDown = glfwGetKey(ctx_.window(), GLFW_KEY_T) == GLFW_PRESS;
		if (tDown && !tWasDown_)
			autoRotate_ = !autoRotate_;
		tWasDown_ = tDown;

		// C reset user scale
		const bool cDown = glfwGetKey(ctx_.window(), GLFW_KEY_C) == GLFW_PRESS;
		if (cDown && !cWasDown_)
			userScale_ = 1.0f;
		cWasDown_ = cDown;

		// +/- scale
		const bool plusDown = (glfwGetKey(ctx_.window(), GLFW_KEY_EQUAL) == GLFW_PRESS) ||
							  (glfwGetKey(ctx_.window(), GLFW_KEY_KP_ADD) == GLFW_PRESS);
		const bool minusDown = (glfwGetKey(ctx_.window(), GLFW_KEY_MINUS) == GLFW_PRESS) ||
							   (glfwGetKey(ctx_.window(), GLFW_KEY_KP_SUBTRACT) == GLFW_PRESS);

		if (plusDown && !plusWasDown_)
			userScale_ = std::min(userScale_ * 1.05f, 50.0f);
		if (minusDown && !minusWasDown_)
			userScale_ = std::max(userScale_ / 1.05f, 0.02f);

		plusWasDown_ = plusDown;
		minusWasDown_ = minusDown;

		// F1 wireframe toggle
		const bool f1Down = glfwGetKey(ctx_.window(), GLFW_KEY_F1) == GLFW_PRESS;
		if (f1Down && !f1WasDown_)
		{
			if (!ctx_.wireframeSupported())
			{
				if (!warnedNoWire_)
				{
					warnedNoWire_ = true;
					std::cerr << "Wireframe not supported (fillModeNonSolid not available).\n";
				}
			}
			else
			{
				wireframe_ = !wireframe_;
				vkDeviceWaitIdle(ctx_.device());

				modelPipe_.recreate(swap_.extent(),
									wireframe_ ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL);

				cmds_.recordScene(modelPipe_.renderPass(),
								  fbs_.get(),
								  swap_.extent(),
								  modelPipe_.pipeline(),
								  modelPipe_.layout(),
								  desc_.sets(),
								  hasModel_ ? vb_.buffer() : VK_NULL_HANDLE,
								  hasModel_ ? ib_.buffer() : VK_NULL_HANDLE,
								  hasModel_ ? ib_.count() : 0u,
								  VK_INDEX_TYPE_UINT32,
								  linesPipe_.pipeline(),
								  linesVB_.buffer(),
								  linesVertexCount_);
			}
		}
		f1WasDown_ = f1Down;

		if (!paused_ && autoRotate_)
			modelTime_ += dt;

		// camera vectors
		const float yaw = degToRad(yawDeg_);
		const float pitch = degToRad(pitchDeg_);

		scop::math::Vec3 forward{
			std::cos(pitch) * std::cos(yaw),
			std::sin(pitch),
			std::cos(pitch) * std::sin(yaw)};
		forward = scop::math::normalize(forward);

		const scop::math::Vec3 worldUp{0.f, 1.f, 0.f};
		scop::math::Vec3 right = scop::math::normalize(scop::math::cross(forward, worldUp));
		scop::math::Vec3 up = scop::math::cross(right, forward);

		float speed = 2.5f;
		if (glfwGetKey(ctx_.window(), GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
			speed *= 3.0f;

		if (glfwGetKey(ctx_.window(), GLFW_KEY_W) == GLFW_PRESS)
		{
			camX_ += forward.x * speed * dt;
			camY_ += forward.y * speed * dt;
			camZ_ += forward.z * speed * dt;
		}
		if (glfwGetKey(ctx_.window(), GLFW_KEY_S) == GLFW_PRESS)
		{
			camX_ -= forward.x * speed * dt;
			camY_ -= forward.y * speed * dt;
			camZ_ -= forward.z * speed * dt;
		}
		if (glfwGetKey(ctx_.window(), GLFW_KEY_A) == GLFW_PRESS)
		{
			camX_ -= right.x * speed * dt;
			camY_ -= right.y * speed * dt;
			camZ_ -= right.z * speed * dt;
		}
		if (glfwGetKey(ctx_.window(), GLFW_KEY_D) == GLFW_PRESS)
		{
			camX_ += right.x * speed * dt;
			camY_ += right.y * speed * dt;
			camZ_ += right.z * speed * dt;
		}
		if (glfwGetKey(ctx_.window(), GLFW_KEY_Q) == GLFW_PRESS)
		{
			camY_ -= speed * dt;
		}
		if (glfwGetKey(ctx_.window(), GLFW_KEY_E) == GLFW_PRESS)
		{
			camY_ += speed * dt;
		}

		// title (FPS + stats)
		fpsAccum_ += dt;
		fpsFrames_ += 1;
		if (fpsAccum_ >= 0.5)
		{
			const double fps = static_cast<double>(fpsFrames_) / fpsAccum_;
			fpsAccum_ = 0.0;
			fpsFrames_ = 0;

			const float appliedScale = (autoFit_ ? fitScale_ : 1.0f) * userScale_;
			const uint32_t tris = hasModel_ ? (modelIndexCount_ / 3u) : 0u;

			std::ostringstream oss;
			oss.setf(std::ios::fixed);
			oss.precision(1);
			oss << "scop | " << modelLabel_
				<< " | FPS " << fps
				<< " | " << (wireframe_ ? "WF" : "FILL")
				<< " | " << (autoFit_ ? "FIT" : "RAW")
				<< " | Scale " << appliedScale
				<< " | BBox " << (showBounds_ ? "ON" : "OFF");

			if (hasModel_)
			{
				oss << " | V " << modelVertexCount_
					<< " | T " << tris;
			}

			oss << " | " << (cursorLocked_ ? "Mouse: LOCK" : "Mouse: FREE");
			glfwSetWindowTitle(ctx_.window(), oss.str().c_str());
		}

		// acquire / update ubo / present
		uint32_t imageIndex = 0;
		if (presenter_.acquire(imageIndex) == FramePresenter::Result::OutOfDate)
		{
			recreateSwapchain();
			return;
		}

		const VkExtent2D ext = swap_.extent();
		const float aspect = (ext.height == 0) ? 1.0f : (static_cast<float>(ext.width) / static_cast<float>(ext.height));

		const scop::math::Mat4 view = scop::math::Mat4::lookAt(
			{camX_, camY_, camZ_},
			{camX_ + forward.x, camY_ + forward.y, camZ_ + forward.z},
			up);

		const scop::math::Mat4 proj = scop::math::Mat4::perspective(degToRad(fovDeg_), aspect, 0.1f, 200.0f, true);
		const scop::math::Mat4 vp = scop::math::Mat4::mul(proj, view);

		const float appliedScale = (autoFit_ ? fitScale_ : 1.0f) * userScale_;
		const scop::math::Mat4 Tfit = autoFit_
										  ? scop::math::Mat4::translation(fitOffsetX_, fitOffsetY_, fitOffsetZ_)
										  : scop::math::Mat4::identity();

		const scop::math::Mat4 S = scop::math::Mat4::scale(appliedScale);

		const scop::math::Mat4 R =
			scop::math::Mat4::mul(scop::math::Mat4::rotationY(modelTime_),
								  scop::math::Mat4::rotationX(modelTime_ * 0.7f));

		const scop::math::Mat4 model = scop::math::Mat4::mul(R, scop::math::Mat4::mul(S, Tfit));

		UBOData u{};
		u.vp = vp;
		u.model = model;

		u.lightDir[0] = 0.6f;
		u.lightDir[1] = -1.0f;
		u.lightDir[2] = 0.4f;
		u.lightDir[3] = 0.0f;
		u.baseColor[0] = 0.82f;
		u.baseColor[1] = 0.85f;
		u.baseColor[2] = 0.92f;
		u.baseColor[3] = 0.0f;
		u.cameraPos[0] = camX_;
		u.cameraPos[1] = camY_;
		u.cameraPos[2] = camZ_;
		u.cameraPos[3] = 0.0f;
		u.spec[0] = 0.55f;
		u.spec[1] = 64.0f;
		u.spec[2] = 0.0f;
		u.spec[3] = 0.0f;

		ubos_.at(imageIndex).update(&u, sizeof(u));

		if (presenter_.submitPresent(imageIndex) == FramePresenter::Result::OutOfDate)
		{
			recreateSwapchain();
			return;
		}
	}

} // namespace scop::vk
