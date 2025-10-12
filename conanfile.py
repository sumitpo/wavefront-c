# conanfile.py
from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.files import copy
import os


class WavefrontParserConan(ConanFile):
    name = "wavefront-parser"
    description = "Complete Wavefront OBJ/MTL parser library for C"
    version = "1.0.0"
    license = "MIT"
    url = "https://github.com/yourname/wavefront-parser"
    homepage = "https://github.com/yourname/wavefront-parser"
    topics = ("wavefront", "obj", "mtl", "3d", "parser", "c")

    package_type = "library"
    settings = "os", "arch", "compiler", "build_type"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "build_examples": [True, False],
        "build_tests": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "build_examples": True,
        "build_tests": True,
    }

    def requirements(self):
        # project depends on cmocka for testing
        self.requires("cmocka/1.1.7")
        self.requires("log4c/1.0.0@local/stable")

    # ====== configure for package ======
    exports_sources = (
        "CMakeLists.txt",
        "include/*",
        "src/*",
        "Config.cmake.in",
        "LICENSE",
    )

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")
        self.settings.rm_safe("compiler.cppstd")
        self.settings.rm_safe("compiler.libcxx")

    def layout(self):
        cmake_layout(self, src_folder=".")

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self)
        # set options based on whether to build test
        tc.variables["BUILD_TESTS"] = not self.conf.get(
            "tools.build:skip_test", default=False
        )
        tc.variables["BUILD_EXAMPLES"] = True
        tc.variables["ENABLE_ASAN"] = True
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure(
            variables={
                "WF_BUILD_EXAMPLES": "ON" if self.options.build_examples else "OFF",
                "WF_BUILD_TESTS": "ON" if self.options.build_tests else "OFF",
            }
        )
        cmake.build()

        # run test if enable
        if (
            not self.conf.get("tools.build:skip_test", default=False)
            and self.options.build_tests
        ):
            print("test is enabled")
            cmake.test()

    def package(self):
        copy(
            self,
            "*.h",
            src=os.path.join(self.source_folder, "include"),
            dst=os.path.join(self.package_folder, "include"),
        )
        copy(
            self,
            "*.a",
            src=self.build_folder,
            dst=os.path.join(self.package_folder, "lib"),
            keep_path=False,
        )
        copy(
            self,
            "*.lib",
            src=self.build_folder,
            dst=os.path.join(self.package_folder, "lib"),
            keep_path=False,
        )
        copy(
            self,
            "*.so",
            src=self.build_folder,
            dst=os.path.join(self.package_folder, "lib"),
            keep_path=False,
        )
        copy(
            self,
            "*.dylib",
            src=self.build_folder,
            dst=os.path.join(self.package_folder, "lib"),
            keep_path=False,
        )
        copy(
            self,
            "*.dll",
            src=self.build_folder,
            dst=os.path.join(self.package_folder, "bin"),
            keep_path=False,
        )

    def package_info(self):
        self.cpp_info.libs = ["wavefront-parser"]
        if self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.system_libs.append("m")
