#ifndef RENDERER_H
#define RENDERER_H

//#include "qvulkanwindow.h"
//#include <QVulkanWindow>
#include <unordered_map>
#include <set>
#include <queue>
#include "GLFWVulkanWindow.h"
#include "vulkanutil.h"
#include "Vertex.h"
#include "visibilitymanager.h"

#include "viewcell.h"

class NirensteinSampler;

struct Settings {
    std::string modelName;
    int viewCellIndex;
};

struct ViewCellGeometry {
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;

    ViewCellGeometry(VkBuffer vertexBuffer, VkDeviceMemory vertexBufferMemory)
        : vertexBuffer(vertexBuffer), vertexBufferMemory(vertexBufferMemory)
    {
    }
};

class VulkanRenderer { // : public QVulkanWindowRenderer {
public:
    glm::vec3 cameraPos;
    glm::vec3 cameraForward;
    glm::vec3 cameraRight;
    glm::vec3 cameraUp;
    VisibilityManager *visibilityManager;
    std::vector<uint32_t> indices;

    VulkanRenderer(GLFWVulkanWindow *w);

    void initResources();
    void initSwapChainResources();
    void releaseSwapChainResources();
    void releaseResources();
    void startNextFrame(
        uint32_t swapChainImageIndex, VkFramebuffer framebuffer, VkCommandBuffer commandBuffer,
        VkRenderPass renderPass
    );
    void toggleShadedRendering();
    void toggleViewCellRendering();
    void toggleRayVisualization();
    void nextCorner();
    void nextViewCell();
    void alignCameraWithViewCellNormal();
    void startVisibilityThread();

private:
    const int NUM_THREADS = 1;
    bool USE_NIRENSTEIN_VISIBILITY_SAMPLING;
    bool USE_NIRENSTEIN_MULTI_VIEW_RENDERING;
    bool USE_NIRENSTEIN_ADAPTIVE_DIVIDE;

    //QVulkanWindow *window;
    GLFWVulkanWindow *window;
    std::map<std::string, std::string> se;

    VkDescriptorPool descriptorPool;
    std::vector<VkDescriptorSet> descriptorSets;
    VkDescriptorSetLayout descriptorSetLayout;
    std::vector<VkDescriptorSet> computeDescriptorSet;
    VkDescriptorSetLayout computeDescriptorSetLayout;
    VkSampler framebufferSampler;

    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;
    VkPipeline rayVisualizationPipeline;
    VkPipelineLayout rayVisualizationPipelineLayout;
    VkPipeline computePipeline;
    VkPipelineLayout computePipelineLayout;
    std::vector<VkCommandBuffer> computeCommandBuffers;
    VkFence fence;
    VkBuffer renderedBuffer;
    VkDeviceMemory renderedBufferMemory;

    std::unordered_map<Vertex, uint32_t> uniqueVertices;
    std::vector<Vertex> vertices;
    std::vector<Vertex> shadedVertices;
    std::vector<std::vector<int>> pvsTriangleIDs;
    std::vector<std::vector<Vertex>> shadedPVS;
    std::vector<ViewCellGeometry> viewCellGeometry;
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkBuffer shadedVertexBuffer;
    VkDeviceMemory shadedVertexBufferMemory;
    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;
    VkBuffer rayVertexBuffer;
    VkDeviceMemory rayVertexBufferMemory;
    VkBuffer errorBuffer;
    VkDeviceMemory errorBufferMemory;

    VkImage errorDepthImage;
    VkDeviceMemory errorDepthImageMemory;
    VkImageView errorDepthImageView;
    VkImage errorColorImage;
    VkDeviceMemory errorColorImageMemory;
    VkImageView errorColorImageView;
    VkSampler errorColorImageSampler;
    VkFramebuffer errorFramebuffer;
    VkRenderPass errorRenderPass;

    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;

    VkImage textureImage;
    VkDeviceMemory textureImageMemory;
    VkImageView textureImageView;
    VkSampler textureSampler;
    int currentViewCellCornerView = 0;

    int currentViewCellIndex = 0;

    void createGraphicsPipeline(
        VkPipeline &pipeline, VkPipelineLayout &pipelineLayout, std::string vertShaderPath,
        std::string fragShaderPath,
        VkPipelineLayoutCreateInfo pipelineLayoutInfo,
        VkPrimitiveTopology primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
    );
    void createVertexBuffer(std::vector<Vertex> &vertices, VkBuffer &vertexBuffer, VkDeviceMemory &vertexBufferMemory);
    void updateVertexBuffer(std::vector<Vertex> &vertices, VkBuffer &vertexBuffer, VkDeviceMemory &vertexBufferMemory);
    void createShadedVertexBuffer();
    void createIndexBuffer();
    void createErrorBuffer();

    void createComputePipeline();
    void createComputeDescriptorSets();
    void createComputeDescriptorLayout();
    void createComputeCommandBuffer();

    void loadModel(std::string modelPath);
    void createTextureImage();
    void createTextureImageView();
    void createTextureSampler();
    void transitionImageLayout(
        VkCommandBuffer commandBuffer, VkImage image, VkFormat format, VkImageLayout oldLayout,
        VkImageLayout newLayout,
        VkPipelineStageFlags sourceStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VkPipelineStageFlags destinationStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT
    );
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

    void createDescriptorSetLayout();
    void createDescriptorPool();
    void createDescriptorSets();
    void createUniformBuffers();
    void updateUniformBuffer(uint32_t swapChainImageIndex);

    // Visibility
    bool shadedRendering = false;
    bool viewCellRendering = false;
    std::string pvsStorageFile;
    bool loadPVS;
    //std::thread visibilityThread;
    std::vector<std::thread> visibilityThreads;
    VkFramebuffer primitiveIDFramebuffer;
    float totalError;
    float maxError;

    void initVisibilityManager();
    std::vector<glm::mat4> loadSceneFile(Settings settings);
    Settings loadSettingsFile();
    float calculateError(const ViewCell &viewCell, const std::vector<glm::vec2> &haltonPoints);

    NirensteinSampler *nirensteinSampler;
    std::vector<glm::vec3> viewCellSizes;
};

#endif // RENDERER_H
