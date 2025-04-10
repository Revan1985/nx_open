// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <QtCore/QCoreApplication>

#include <nx/utils/test_support/run_test.h>
#include <nx/vms/rules/metatypes.h>

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);

    return nx::utils::test::runTest(
        argc,
        argv,
        [](const nx::ArgumentParser& /*args*/)
        {
            nx::vms::rules::Metatypes::initialize();
            return nx::utils::test::DeinitFunctions{};
        },
        nx::utils::test::TestRunFlag::throwOnFailure);
}
