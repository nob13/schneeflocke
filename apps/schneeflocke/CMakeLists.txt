
message (STATUS "Qt 4")
	find_package (Qt4 COMPONENTS QtMain QtCore QtGui QtXml)

if (QT4_FOUND)
	include (${QT_USE_FILE})
	set (LIBS ${LIBS} ${QT_LIBRARIES})

	# Collecting all files	
	file (GLOB_RECURSE src_files    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} src/*.cpp)
	file (GLOB_RECURSE header_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} src/*.h)
	file (GLOB_RECURSE ui_files     RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} src/gui/ui/*.ui)

	message (STATUS "Ui Files: ${ui_files}")

	# QObject based files (need moc-wrapping), if we use all headers, we generate a lot of warnings
	set (qobj_files "src/gui/GUI.h" "src/gui/ShareFileDialog.h" "src/QtThreadGateway.h" "src/gui/SettingsDialog.h" "src/gui/TransferWindow.h" "src/gui/TrayIcon.h") 

	QT4_WRAP_UI (ui_headers ${ui_files})
	# message (STATUS "Ui Headers: ${ui_headers}")

	set (QTMOC_BUG_WORKAROUND OPTIONS -DBOOST_TT_HAS_OPERATOR_HPP_INCLUDED) # Bug 22829

	QT4_WRAP_CPP (moc_files ${qobj_files} ${QTMOC_BUG_WORKAROUND})
	# message (STATUS "MOC Files: ${moc_files}")
	
	QT4_ADD_RESOURCES (icon_srcs icons/icons.qrc)
	# message (STATUS "Icons: ${icon_files}")
	message (STATUS "Encoded Icon Resources: ${icon_srcs}")

	# WIN32 program start icon
	if (WIN32)
	set (icon_srcs ${icon_srcs} ${CMAKE_CURRENT_SOURCE_DIR}/src/schneeflocke.rc) 
	endif()


	include_directories (${CMAKE_CURRENT_BINARY_DIR})  # Place of UIC Files
	# message (STATUS "ALL COMINED SOURCE FILES ${src_files} ${ui_headers} ${moc_files} ${icon_srcs}")
	add_executable (schneeflocke WIN32 ${src_files} ${ui_headers} ${moc_files} ${icon_srcs}) # Qt4's uic just generates src-files
	target_link_libraries (schneeflocke ${LIBS})
	install (TARGETS schneeflocke RUNTIME DESTINATION bin)

	if (WIN32)
	# Qt4 .dll files
	set (QT4_DLL_FILES ${QT_BINARY_DIR}/QtGui4.dll ${QT_BINARY_DIR}/QtCore4.dll)
	set (QT4_DLL_FILES_DEBUG ${QT_BINARY_DIR}/QtGuid4.dll ${QT_BINARY_DIR}/QtCored4.dll)
	install (FILES ${QT4_DLL_FILES} DESTINATION bin)
	install (FILES ${QT4_DLL_FILES_DEBUG} CONFIGURATIONS Debug DESTINATION bin) # Debug DLLs
	endif()
	if (LINUX)
	# Startmenu entry and icons
	install (FILES schneeflocke.desktop DESTINATION share/applications)
	install (FILES icons/schneeflocke.png DESTINATION share/icons)
	endif()
else ()
	message (Status "Did not Found Qt4 - not building schneeflocke (GUI)")
endif()

