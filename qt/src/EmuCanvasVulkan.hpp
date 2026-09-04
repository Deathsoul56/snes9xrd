#pragma once
#include <QWindow>

#include "EmuCanvas.hpp"
#include "ShaderParametersDialog.hpp"
#include "common/video/vulkan/vulkan_simple_output.hpp"
#include "common/video/vulkan/vulkan_shader_chain.hpp"
#include "common/video/vulkan/vulkan_texture.hpp"

#ifndef _WIN32
#include "common/video/wayland/wayland_surface.hpp"
#endif

class EmuCanvasVulkan : public EmuCanvas
{
  public:
    EmuCanvasVulkan(EmuConfig *config, QWidget *main_window);

    bool createContext() override;
    void deinit() override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    QPaintEngine *paintEngine() const override { return nullptr; }

    std::vector<std::string> getDeviceList() const override;
    void shaderChanged() override;
    void showParametersDialog() override;
    void saveParameters(std::string filename) override;
    void signalInputStage() override;

    void draw() override;

    bool initImGui();
    void recreateUIAssets() override;
    vk::UniqueDescriptorPool imgui_descriptor_pool;

    std::unique_ptr<Vulkan::Context> context;
    std::unique_ptr<Vulkan::SimpleOutput> simple_output;
    std::unique_ptr<Vulkan::ShaderChain> shader_chain;

  private:
    void tryLoadShader();
    void destroyAchievementBadge();
    std::string current_shader;
    QWindow *window = nullptr;
    std::unique_ptr<ShaderParametersDialog> shader_parameters_dialog = nullptr;
    QString platform;

    std::unique_ptr<Vulkan::Texture> achievement_badge_texture;
    VkDescriptorSet achievement_badge_descriptor = VK_NULL_HANDLE;
    uint32_t last_seen_badge_generation = 0;

#ifndef _WIN32
    std::unique_ptr<WaylandSurface> wayland_surface;
#endif
};
