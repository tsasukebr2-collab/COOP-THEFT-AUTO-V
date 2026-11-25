#include "TestMain.h"

void RunProtocolTests();
void RunSaveTests();

int main() {
    RunProtocolTests();
    RunSaveTests();
    return TestReport("GTAV-Coop test suite");
}
