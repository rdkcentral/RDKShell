

add_library( RdkShellExtendedInputWesterosPlugin

        SHARED
        source/rdkshellextendedinputmodule.cpp
        protocol/rdkshell_extended_input-protocol.c

        )

target_link_libraries( RdkShellExtendedInputWesterosPlugin rdkshell_shared)

target_include_directories( RdkShellExtendedInputWesterosPlugin

        PRIVATE
        protocol

        $<TARGET_PROPERTY:rdkshell_shared,INTERFACE_INCLUDE_DIRECTORIES>

        )

set_target_properties( RdkShellExtendedInputWesterosPlugin

        PROPERTIES
        OUTPUT_NAME "westeros_plugin_rdkshell_extended_input"
        VERSION     1.0.0
        SOVERSION   1

        )

install( TARGETS RdkShellExtendedInputWesterosPlugin
         DESTINATION lib/plugins/westeros/

        )
