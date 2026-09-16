from pathlib import Path
import importlib.util
import shutil

from setuptools import setup
from setuptools.command.build_py import build_py


class BuildWithSchemas(build_py):
    def run(self):
        super().run()
        source = Path(__file__).parent / "schemas"
        destination = Path(self.build_lib) / "uran_interfaces" / "schemas"
        shutil.copytree(source, destination, dirs_exist_ok=True)
        generator_path = Path(__file__).parent / "generators" / "generate_catalog.py"
        spec = importlib.util.spec_from_file_location("uran_catalog_generator", generator_path)
        generator = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(generator)
        generator.generate_python(source, Path(self.build_lib) / "uran_interfaces" / "_catalog.py")


setup(cmdclass={"build_py": BuildWithSchemas})
