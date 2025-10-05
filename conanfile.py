# conanfile.py
from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.files import copy
import os


class WavefrontParserConan(ConanFile):
    name = "wavefront-parser"
    description = "Complete Wavefront OBJ/MTL parser library for C"
    license = "MIT"
    url = "https://github.com/yourname/wavefront-parser"
    homepage = "https://github.com/yourname/wavefront-parser"
    topics = ("wavefront", "obj", "mtl", "3d", "parser", "c")

    package_type = "library"
    settings = "os", "arch", "compiler", "build_type"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
    }

    # ====== 消费依赖（原 conanfile.txt 的内容）======
    def requirements(self):
        # 你的项目依赖 cmocka（用于测试）
        self.requires("cmocka/1.1.7")

    # ====== 创建包的配置 ======
    exports_sources = "CMakeLists.txt", "include/*", "src/*"

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
        # 根据是否构建测试来设置选项
        tc.variables["BUILD_TESTS"] = not self.conf.get(
            "tools.build:skip_test", default=False
        )
        tc.variables["BUILD_EXAMPLES"] = True
        tc.variables["ENABLE_ASAN"] = True
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

        # 运行测试（如果启用）
        if not self.conf.get("tools.build:skip_test", default=False):
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
