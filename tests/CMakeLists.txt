function(zs_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE zapshell_core Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endfunction()

zs_test(tst_cli)
zs_test(tst_paths)

target_compile_definitions(tst_cli PRIVATE ZAPSHELL_BIN="$<TARGET_FILE:zapshell>")
add_dependencies(tst_cli zapshell)

zs_test(tst_web)
zs_test(tst_instance)
target_link_libraries(tst_instance PRIVATE Qt6::Network)
zs_test(tst_notify)
zs_test(tst_backup)
zs_test(tst_stats)

add_test(NAME tst_icon COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tst_icon.sh)

add_executable(tst_tray_toggle tst_tray_toggle.cpp)
target_link_libraries(tst_tray_toggle PRIVATE zapshell_ui Qt6::Widgets Qt6::Test)
add_test(NAME tst_tray_toggle COMMAND tst_tray_toggle)
set_tests_properties(tst_tray_toggle PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
