include("E:/HMI_SerialDashboard/build/Desktop_Qt_6_11_0_MinGW_64_bit-Debug/.qt/QtDeploySupport.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/HMI_SerialDashboard-plugins.cmake" OPTIONAL)
set(__QT_DEPLOY_I18N_CATALOGS "qtbase")

qt6_deploy_runtime_dependencies(
    EXECUTABLE "E:/HMI_SerialDashboard/build/Desktop_Qt_6_11_0_MinGW_64_bit-Debug/HMI_SerialDashboard.exe"
    GENERATE_QT_CONF
)
