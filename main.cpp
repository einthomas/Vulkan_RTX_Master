#include <QGuiApplication>
#include <QtGlobal>
#include <QVulkanInstance>
#include <iostream>
#include <QLoggingCategory>

#include "Window.h"
#include "Renderer.h"

int main(int argc, char *argv[])
{    
    QGuiApplication app(argc, argv);

    QVulkanInstance vulkanInstance;

    if (qEnvironmentVariableIntValue("QT_VK_DEBUG")) {
        QLoggingCategory::setFilterRules(QStringLiteral("qt.vulkan=true"));
        vulkanInstance.setLayers(QByteArrayList() << "VK_LAYER_LUNARG_standard_validation");
    }

    if (!vulkanInstance.create()) {
        qWarning("failed to create Vulkan instance");
    }

    Window window;
    window.setDeviceExtensions(
        QByteArrayList()
        << VK_NV_RAY_TRACING_EXTENSION_NAME
        << VK_KHR_MAINTENANCE3_EXTENSION_NAME
        << VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME
    );
    window.setVulkanInstance(&vulkanInstance);
    window.resize(800, 600);
    window.show();

    return app.exec();
}
