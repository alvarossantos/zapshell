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
