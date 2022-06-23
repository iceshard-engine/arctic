import Project from require 'ice.workspace.project'
import Arctic from require 'tools.arctic'

with Project "Arctic"
    \application Arctic
    \script "arctic.bat"
    \profiles "source/conan_profiles.json"
    \fastbuild_script "source/fbuild.bff"

    \output "build"
    \sources "source/code"
    \working_dir "build"
    \finish!
