workspace "Networking"
	architecture "x64"
	startproject "App"
	
	configurations{
		"Debug",
		"Release",
		"Dist"
	}
	
	flags
	{
		"MultiProcessorCompile"
	}

outputdir = "%{cfg.system}%{cfg.architecture}-%{cfg.buildcfg}"
 
group "Core"
	include "Core"
group ""

group "App"
	include "App"
group ""

