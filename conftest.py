"""Command-line options shared by the target and bench tiers (pytest reads options only from the root)."""
import os


def pytest_addoption(parser):
    parser.addoption("--coldflash-dir", default=os.environ.get("COLDFLASH_DIR"),
                     help="CI artefact coldflash/ directory (flash_args + images)")
    parser.addoption("--expect-version", default=os.environ.get("EXPECT_VERSION"),
                     help="version the DUT must report")
