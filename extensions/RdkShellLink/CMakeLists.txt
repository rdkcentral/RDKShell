add_library(RdkShellLink
        SHARED
        rdkshelllink.cpp
        )

set_target_properties(RdkShellLink
        PROPERTIES
        OUTPUT_NAME "rdkshell_link"
#        VERSION     1.0.0
#        SOVERSION   1
        )

# install(TARGETS RdkShellLink
#         DESTINATION lib/plugins/westeros/
#         )