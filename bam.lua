--[[
   Simple http-client written to be an drop-in code, focus is to be small
   and easy to use but maybe not efficient.

   version 1.0, June, 2014

   Copyright (C) 2012- Fredrik Kihlander

   This software is provided 'as-is', without any express or implied
   warranty.  In no event will the authors be held liable for any damages
   arising from the use of this software.

   Permission is granted to anyone to use this software for any purpose,
   including commercial applications, and to alter it and redistribute it
   freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
      claim that you wrote the original software. If you use this software
      in a product, an acknowledgment in the product documentation would be
      appreciated but is not required.
   2. Altered source versions must be plainly marked as such, and must not be
      misrepresented as being the original software.
   3. This notice may not be removed or altered from any source distribution.

   Fredrik Kihlander
--]]

BUILD_PATH = "local"

config   = "debug"

local settings = NewSettings() -- {}

settings.cc.includes:Add("include")
if family == 'windows' then
	platform = "winx64"
	settings.cc.flags:Add( "/TP" ) -- forcing c++ compile on windows =/
	settings.link.libs:Add( "Ws2_32" )
else
	platform = "linux_x86_64"
	settings.cc.flags:Add( "-Wconversion", "-Wextra", "-Wall", "-Werror", "-Wstrict-aliasing=2" )
end

local output_path = PathJoin( BUILD_PATH, PathJoin( platform, config ) )
local output_func = function(settings, path) return PathJoin(output_path, PathFilename(PathBase(path)) .. settings.config_ext) end
settings.cc.Output = output_func
settings.lib.Output = output_func
settings.link.Output = output_func

local objs  = Compile( settings, Collect( 'src/*.cpp' ) )
local lib   = StaticLibrary( settings, 'http_client', objs )

local example = Link( settings, 'http_tester',     Compile( settings, 'test/http_tester.cpp' ), lib )
