#include "scop/vk/Renderer.hpp"
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
		float texMix[4];
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

	static void pushLine(std::vector<scop::vk::Vertex> &out,
						 float x1, float y1, float z1,
						 float x2, float y2, float z2,
						 float r, float g, float b)
	{
		scop::vk::Vertex a{};
		a.pos[0] = x1;
		a.pos[1] = y1;
		a.pos[2] = z1;
		a.nrm[0] = r;
		a.nrm[1] = g;
		a.nrm[2] = b;
		a.uv[0] = 0.f;
		a.uv[1] = 0.f;

		scop::vk::Vertex c{};
		c.pos[0] = x2;
		c.pos[1] = y2;
		c.pos[2] = z2;
		c.nrm[0] = r;
		c.nrm[1] = g;
		c.nrm[2] = b;
		c.uv[0] = 0.f;
		c.uv[1] = 0.f;

		out.push_back(a);
		out.push_back(c);
	}

	static std::vector<scop::vk::Vertex> makeGridAxes(int half = 10, float step = 1.0f)
	{
		std::vector<scop::vk::Vertex> out;

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

	Renderer::Renderer(int width, int height, const char *title) { init(width, height, title, std::string()); }
	Renderer::Renderer(int width, int height, const char *title, const std::string &initialObjPath) { init(width, height, title, initialObjPath); }

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

			const float x0 = minX, x1 = maxX;
			const float y0 = minY, y1 = maxY;
			const float z0 = minZ, z1 = maxZ;

			pushLine(lines, x0, y0, z0, x1, y0, z0, r, g, b);
			pushLine(lines, x1, y0, z0, x1, y0, z1, r, g, b);
			pushLine(lines, x1, y0, z1, x0, y0, z1, r, g, b);
			pushLine(lines, x0, y0, z1, x0, y0, z0, r, g, b);

			pushLine(lines, x0, y1, z0, x1, y1, z0, r, g, b);
			pushLine(lines, x1, y1, z0, x1, y1, z1, r, g, b);
			pushLine(lines, x1, y1, z1, x0, y1, z1, r, g, b);
			pushLine(lines, x0, y1, z1, x0, y1, z0, r, g, b);

			pushLine(lines, x0, y0, z0, x0, y1, z0, r, g, b);
			pushLine(lines, x1, y0, z0, x1, y1, z0, r, g, b);
			pushLine(lines, x1, y0, z1, x1, y1, z1, r, g, b);
			pushLine(lines, x0, y0, z1, x0, y1, z1, r, g, b);
		}

		linesVertexCount_ = static_cast<uint32_t>(lines.size());
		linesVB_ = VertexBuffer(
			ctx_.device(),
			ctx_.physicalDevice(),
			ctx_.indices().graphicsFamily.value(),
			ctx_.graphicsQueue(),
			lines);
	}

	bool Renderer::loadModelFromPath(const std::string &path)
	{
		scop::io::MeshData mesh;
		std::string texPath;

		try
		{
			mesh = scop::io::loadObj(path, true);
			texPath = mesh.material.mapKd;

			matKd_[0] = mesh.material.Kd[0];
			matKd_[1] = mesh.material.Kd[1];
			matKd_[2] = mesh.material.Kd[2];
			matAlpha_ = mesh.material.d;

			const float ksAvg = (mesh.material.Ks[0] + mesh.material.Ks[1] + mesh.material.Ks[2]) / 3.0f;
			matSpecStrength_ = std::clamp(ksAvg, 0.0f, 1.0f);
			matShininess_ = std::max(mesh.material.Ns, 1.0f);
		}
		catch (const std::exception &e)
		{
			std::cerr << "OBJ load failed: " << e.what() << "\n";
			hasModel_ = false;
			modelVertexCount_ = 0;
			modelIndexCount_ = 0;
			modelLabel_ = "Drop .obj onto window";
			return false;
		}

		if (mesh.vertices.empty() || mesh.indices.empty())
		{
			std::cerr << "OBJ has no geometry.\n";
			hasModel_ = false;
			modelVertexCount_ = 0;
			modelIndexCount_ = 0;
			modelLabel_ = "Drop .obj onto window";
			return false;
		}

		modelVertexCount_ = static_cast<uint32_t>(mesh.vertices.size());
		modelIndexCount_ = static_cast<uint32_t>(mesh.indices.size());

		// AABB in model space
		{
			float minX = std::numeric_limits<float>::infinity();
			float minY = std::numeric_limits<float>::infinity();
			float minZ = std::numeric_limits<float>::infinity();
			float maxX = -std::numeric_limits<float>::infinity();
			float maxY = -std::numeric_limits<float>::infinity();
			float maxZ = -std::numeric_limits<float>::infinity();

			for (const auto &v : mesh.vertices)
			{
				minX = std::min(minX, v.pos[0]);
				minY = std::min(minY, v.pos[1]);
				minZ = std::min(minZ, v.pos[2]);
				maxX = std::max(maxX, v.pos[0]);
				maxY = std::max(maxY, v.pos[1]);
				maxZ = std::max(maxZ, v.pos[2]);
			}

			aabbMin_[0] = minX;
			aabbMin_[1] = minY;
			aabbMin_[2] = minZ;
			aabbMax_[0] = maxX;
			aabbMax_[1] = maxY;
			aabbMax_[2] = maxZ;

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

		// Per-model defaults
		autoRotate_ = false;
		paused_ = false;
		modelTime_ = 0.0f;

		orbitTargetX_ = 0.0f;
		orbitTargetY_ = 0.0f;
		orbitTargetZ_ = 0.0f;
		orbitDistance_ = 4.0f;

		vkDeviceWaitIdle(ctx_.device());

		// Geometry buffers
		vb_ = VertexBuffer(ctx_.device(), ctx_.physicalDevice(),
						   ctx_.indices().graphicsFamily.value(), ctx_.graphicsQueue(),
						   mesh.vertices);

		ib_ = IndexBuffer(ctx_.device(), ctx_.physicalDevice(),
						  ctx_.indices().graphicsFamily.value(), ctx_.graphicsQueue(),
						  mesh.indices);

		hasModel_ = true;
		modelLabel_ = baseName(path);

		// Texture (from MTL map_Kd)
		texPath = mesh.material.mapKd;
		try
		{
			if (!texPath.empty())
			{
				tex_.load(ctx_.device(), ctx_.physicalDevice(),
						  ctx_.indices().graphicsFamily.value(), ctx_.graphicsQueue(),
						  texPath);
				texLabel_ = baseName(texPath);
				showTexture_ = !texPath.empty(); // if no texture in MTL, start in color mode
				texMixTarget_ = showTexture_ ? 1.0f : 0.0f;
				texMix_ = texMixTarget_;
			}
			else
			{
				// no map_Kd in .mtl => keep white texture, but still use Kd color from MTL in shader
				tex_.makeWhite(ctx_.device(), ctx_.physicalDevice(),
							   ctx_.indices().graphicsFamily.value(), ctx_.graphicsQueue());
				texLabel_ = "(none)";
			}
		}
		catch (const std::exception &e)
		{
			std::cerr << "Texture load failed: " << e.what() << "\n";
			tex_.makeWhite(ctx_.device(), ctx_.physicalDevice(),
						   ctx_.indices().graphicsFamily.value(), ctx_.graphicsQueue());
			texLabel_ = "white";
		}

		// If descriptors already exist, update binding 1
		if (!desc_.sets().empty())
			desc_.updateTexture(tex_.view(), tex_.sampler());

		// bbox lines
		if (showBounds_)
			rebuildDebugLines();

		// re-record scene if live
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
				  << " idx=" << mesh.indices.size()
				  << " tex=" << (texPath.empty() ? "(none)" : texPath)
				  << "\n";
		return true;
	}

	void Renderer::init(int width, int height, const char *title) { init(width, height, title, std::string()); }

	void Renderer::init(int width, int height, const char *title, const std::string &initialObjPath)
	{
		ctx_.init(width, height, title);
		glfwSetWindowUserPointer(ctx_.window(), this);

		// Always have a valid texture (white)
		tex_.makeWhite(ctx_.device(), ctx_.physicalDevice(),
					   ctx_.indices().graphicsFamily.value(), ctx_.graphicsQueue());

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

				if (self->orbitMode_)
				{
					const float factor = (yoff > 0.0) ? 0.9f : 1.1f;
					self->orbitDistance_ = std::clamp(self->orbitDistance_ * factor, 0.25f, 80.0f);
				}
				else
				{
					self->fovDeg_ -= static_cast<float>(yoff) * 2.0f;
					self->fovDeg_ = std::clamp(self->fovDeg_, 20.0f, 90.0f);
				}
			});

		glfwSetInputMode(ctx_.window(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		cursorLocked_ = true;

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
				if (!self)
					return;

				const bool lmb = (glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);
				const bool rmb = (glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS);

				// FPS: rotate whenever cursor is locked
				// ORBIT + cursor free: rotate only while LMB, pan only while RMB
				bool rotate = self->cursorLocked_;
				bool pan = false;

				if (self->orbitMode_ && !self->cursorLocked_)
				{
					rotate = lmb;
					pan = rmb;
				}

				if (!rotate && !pan)
				{
					self->firstMouse_ = true;
					return;
				}

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

				if (rotate)
				{
					const float sens = 0.12f;
					self->yawDeg_ += static_cast<float>(dx) * sens;
					self->pitchDeg_ -= static_cast<float>(dy) * sens;
					self->pitchDeg_ = std::clamp(self->pitchDeg_, -89.0f, 89.0f);
				}
				else if (pan && self->orbitMode_)
				{
					// Pan orbit target using camera right/up vectors
					const float yaw = degToRad(self->yawDeg_);
					const float pitch = degToRad(self->pitchDeg_);

					scop::math::Vec3 forward{
						std::cos(pitch) * std::cos(yaw),
						std::sin(pitch),
						std::cos(pitch) * std::sin(yaw)};
					forward = scop::math::normalize(forward);

					const scop::math::Vec3 worldUp{0.f, 1.f, 0.f};
					scop::math::Vec3 right = scop::math::normalize(scop::math::cross(forward, worldUp));
					scop::math::Vec3 up = scop::math::cross(right, forward);

					const float panSens = 0.0020f * std::max(self->orbitDistance_, 1.0f);

					self->orbitTargetX_ += (-right.x * static_cast<float>(dx) + up.x * static_cast<float>(dy)) * panSens;
					self->orbitTargetY_ += (-right.y * static_cast<float>(dx) + up.y * static_cast<float>(dy)) * panSens;
					self->orbitTargetZ_ += (-right.z * static_cast<float>(dx) + up.z * static_cast<float>(dy)) * panSens;
				}
			});

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
			});

		hasModel_ = false;
		modelLabel_ = "Drop .obj onto window";
		texLabel_ = "white";

		rebuildDebugLines();

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
				std::cerr << "Initial path not a valid .obj: " << initialObjPath << "\n";
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

		// Descriptor layout: binding0 UBO + binding1 sampler
		desc_ = Descriptors(
			ctx_.device(),
			uboBuffers,
			sizeof(UBOData),
			tex_.view(),
			tex_.sampler());

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

		// TAB toggles FPS <-> ORBIT
		const bool tabDown = glfwGetKey(ctx_.window(), GLFW_KEY_TAB) == GLFW_PRESS;
		if (tabDown && !tabWasDown_)
		{
			orbitMode_ = !orbitMode_;
			cursorLocked_ = !orbitMode_;
			glfwSetInputMode(ctx_.window(), GLFW_CURSOR,
							 cursorLocked_ ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
			firstMouse_ = true;
		}
		tabWasDown_ = tabDown;

		// ESC toggles mouse lock
		const bool escDown = glfwGetKey(ctx_.window(), GLFW_KEY_ESCAPE) == GLFW_PRESS;
		if (escDown && !escWasDown_)
		{
			cursorLocked_ = !cursorLocked_;
			glfwSetInputMode(ctx_.window(), GLFW_CURSOR,
							 cursorLocked_ ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
			firstMouse_ = true;
		}
		escWasDown_ = escDown;

		// R reset camera
		const bool rDown = glfwGetKey(ctx_.window(), GLFW_KEY_R) == GLFW_PRESS;
		if (rDown && !rWasDown_)
		{
			camX_ = 0.f;
			camY_ = 0.f;
			camZ_ = 2.5f;
			yawDeg_ = -90.f;
			pitchDeg_ = 0.f;
			fovDeg_ = 55.f;
			orbitTargetX_ = 0.f;
			orbitTargetY_ = 0.f;
			orbitTargetZ_ = 0.f;
			orbitDistance_ = 4.f;
			firstMouse_ = true;
		}
		rWasDown_ = rDown;

		// B bbox
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

		// T toggles textured vs flat (smooth fade)
		const bool tDown = glfwGetKey(ctx_.window(), GLFW_KEY_T) == GLFW_PRESS;
		if (tDown && !tWasDown_)
		{
			showTexture_ = !showTexture_;
			texMixTarget_ = showTexture_ ? 1.0f : 0.0f;
		}
		tWasDown_ = tDown;

		// Smoothly approach target (nice exponential ease)
		{
			const float speed = 10.0f; // higher = faster fade
			const float k = 1.0f - std::exp(-speed * dt);
			texMix_ = texMix_ + (texMixTarget_ - texMix_) * k;
			texMix_ = std::clamp(texMix_, 0.0f, 1.0f);
		}

		// F1 wireframe toggle
		const bool f1Down = glfwGetKey(ctx_.window(), GLFW_KEY_F1) == GLFW_PRESS;
		if (f1Down && !f1WasDown_)
		{
			if (!ctx_.wireframeSupported())
			{
				if (!warnedNoWire_)
				{
					warnedNoWire_ = true;
					std::cerr << "Wireframe not supported.\n";
				}
			}
			else
			{
				wireframe_ = !wireframe_;
				vkDeviceWaitIdle(ctx_.device());
				modelPipe_.recreate(swap_.extent(), wireframe_ ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL);
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

		if (autoRotate_)
			modelTime_ += dt;

		// forward/right/up from yaw/pitch
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

		float camx = camX_, camy = camY_, camz = camZ_;

		if (!orbitMode_)
		{
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

			camx = camX_;
			camy = camY_;
			camz = camZ_;
		}
		else
		{
			const float panSpeed = 1.5f * dt * std::max(orbitDistance_, 1.0f);

			scop::math::Vec3 fflat{forward.x, 0.f, forward.z};
			if (std::fabs(fflat.x) + std::fabs(fflat.z) > 0.000001f)
				fflat = scop::math::normalize(fflat);

			if (glfwGetKey(ctx_.window(), GLFW_KEY_W) == GLFW_PRESS)
			{
				orbitTargetX_ += fflat.x * panSpeed;
				orbitTargetZ_ += fflat.z * panSpeed;
			}
			if (glfwGetKey(ctx_.window(), GLFW_KEY_S) == GLFW_PRESS)
			{
				orbitTargetX_ -= fflat.x * panSpeed;
				orbitTargetZ_ -= fflat.z * panSpeed;
			}
			if (glfwGetKey(ctx_.window(), GLFW_KEY_A) == GLFW_PRESS)
			{
				orbitTargetX_ -= right.x * panSpeed;
				orbitTargetZ_ -= right.z * panSpeed;
			}
			if (glfwGetKey(ctx_.window(), GLFW_KEY_D) == GLFW_PRESS)
			{
				orbitTargetX_ += right.x * panSpeed;
				orbitTargetZ_ += right.z * panSpeed;
			}
			if (glfwGetKey(ctx_.window(), GLFW_KEY_Q) == GLFW_PRESS)
			{
				orbitTargetY_ -= panSpeed;
			}
			if (glfwGetKey(ctx_.window(), GLFW_KEY_E) == GLFW_PRESS)
			{
				orbitTargetY_ += panSpeed;
			}

			camx = orbitTargetX_ - forward.x * orbitDistance_;
			camy = orbitTargetY_ - forward.y * orbitDistance_;
			camz = orbitTargetZ_ - forward.z * orbitDistance_;
		}

		// title
		fpsAccum_ += dt;
		fpsFrames_ += 1;
		if (fpsAccum_ >= 0.5)
		{
			const double fps = (double)fpsFrames_ / fpsAccum_;
			fpsAccum_ = 0.0;
			fpsFrames_ = 0;

			const float appliedScale = (autoFit_ ? fitScale_ : 1.0f) * userScale_;
			const uint32_t tris = hasModel_ ? (modelIndexCount_ / 3u) : 0u;

			std::ostringstream oss;
			oss.setf(std::ios::fixed);
			oss.precision(1);

			oss << "scop | " << modelLabel_
				<< " | TEX " << texLabel_ << (showTexture_ ? " ON" : " OFF")
				<< " | " << (orbitMode_ ? "ORBIT" : "FPS")
				<< " | FPS " << fps
				<< " | " << (wireframe_ ? "WF" : "FILL")
				<< " | Scale " << appliedScale
				<< " | BBox " << (showBounds_ ? "ON" : "OFF");

			if (hasModel_)
				oss << " | V " << modelVertexCount_ << " | T " << tris;

			oss << " | " << (cursorLocked_ ? "Mouse: LOCK" : "Mouse: FREE");
			glfwSetWindowTitle(ctx_.window(), oss.str().c_str());
		}

		// acquire
		uint32_t imageIndex = 0;
		if (presenter_.acquire(imageIndex) == FramePresenter::Result::OutOfDate)
		{
			recreateSwapchain();
			return;
		}

		const VkExtent2D ext = swap_.extent();
		const float aspect = (ext.height == 0) ? 1.0f : ((float)ext.width / (float)ext.height);

		scop::math::Mat4 view;
		if (!orbitMode_)
		{
			view = scop::math::Mat4::lookAt({camx, camy, camz},
											{camx + forward.x, camy + forward.y, camz + forward.z},
											up);
		}
		else
		{
			view = scop::math::Mat4::lookAt({camx, camy, camz},
											{orbitTargetX_, orbitTargetY_, orbitTargetZ_},
											up);
		}

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
		u.baseColor[0] = matKd_[0];
		u.baseColor[1] = matKd_[1];
		u.baseColor[2] = matKd_[2];
		u.baseColor[3] = matAlpha_;
		u.cameraPos[0] = camx;
		u.cameraPos[1] = camy;
		u.cameraPos[2] = camz;
		u.cameraPos[3] = 0.0f;
		u.spec[0] = matSpecStrength_;
		u.spec[1] = matShininess_;
		u.spec[2] = 0.0f;
		u.spec[3] = 0.0f;
		u.texMix[0] = texMix_;
		u.texMix[1] = 0.0f;
		u.texMix[2] = 0.0f;
		u.texMix[3] = 0.0f;

		ubos_.at(imageIndex).update(&u, sizeof(u));

		if (presenter_.submitPresent(imageIndex) == FramePresenter::Result::OutOfDate)
		{
			recreateSwapchain();
			return;
		}
	}

} // namespace scop::vk
