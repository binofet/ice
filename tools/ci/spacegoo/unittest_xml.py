"""Run a discovered Python suite with real per-case timing and bounded JUnit failures."""

from pathlib import Path
import sys
import time
import unittest
import json
import xml.etree.ElementTree as ET


class Result(unittest.TextTestResult):
    def __init__(self, *args):
        super().__init__(*args)
        self.xml = ET.Element("testsuite", name="Python conformance")
        self.case = None

    def startTest(self, test):
        super().startTest(test)
        self.clock = time.monotonic()
        self.case = ET.SubElement(self.xml, "testcase", name=str(test), classname=test.__class__.__name__)

    def stopTest(self, test):
        self.case.set("time", str(time.monotonic() - self.clock))
        super().stopTest(test)

    def addFailure(self, test, error):
        super().addFailure(test, error)
        ET.SubElement(self.case, "failure").text = self._exc_info_to_string(error, test)[-16384:]

    def addError(self, test, error):
        super().addError(test, error)
        ET.SubElement(self.case, "error").text = self._exc_info_to_string(error, test)[-16384:]

    def addSkip(self, test, reason):
        super().addSkip(test, reason)
        ET.SubElement(self.case, "skipped", message=str(reason)[:2000])

    def addUnexpectedSuccess(self, test):
        super().addUnexpectedSuccess(test)
        ET.SubElement(self.case, "failure", message="Unexpected success")

    def addExpectedFailure(self, test, error):
        super().addExpectedFailure(test, error)
        ET.SubElement(self.case, "skipped", message="Expected failure is not required green coverage")

    def addSubTest(self, test, subtest, error):
        super().addSubTest(test, subtest, error)
        if error is not None:
            ET.SubElement(self.case, "failure", message=str(subtest)[:2000]).text = (
                self._exc_info_to_string(error, test)[-16384:])


def main(argv):
    if len(argv) != 2:
        raise SystemExit("usage: unittest_xml.py SOURCE_DIRECTORY OUTPUT_XML")
    suite = unittest.defaultTestLoader.discover(str(Path(argv[0]).resolve()))
    def identities(node):
        if isinstance(node, unittest.TestSuite):
            return [name for child in node for name in identities(child)]
        return [str(node)]
    inventory = identities(suite)
    Path(argv[1]).with_suffix(".inventory.json").write_text(json.dumps(inventory) + "\n")
    result = unittest.TextTestRunner(verbosity=2, resultclass=Result).run(suite)
    result.xml.set("tests", str(result.testsRun))
    ET.ElementTree(result.xml).write(argv[1], encoding="utf-8", xml_declaration=True)
    return 0 if result.testsRun and result.wasSuccessful() and not result.skipped and not result.expectedFailures else 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
