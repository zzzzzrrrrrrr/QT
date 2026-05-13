include("E:/HMI_SerialDashboard/build/Release/.qt/QtDeploySupport.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/HMI_SerialDashboard-plugins.cmake" OPTIONAL)
set(__QT_DEPLOY_I18N_CATALOGS "qtbase")

qt6_deploy_runtime_dependencies(
    EXECUTABLE "E:/HMI_SerialDashboard/build/Release/HMI_SerialDashboard.exe"
    GENERATE_QT_CONF
)
