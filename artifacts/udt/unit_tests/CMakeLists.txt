nx_add_test(udt_ut
    WERROR
    PUBLIC_LIBS
        udt nx_utils nx_network
    PROJECT NXLIB
    SUITES
    FOLDER common/tests
)
