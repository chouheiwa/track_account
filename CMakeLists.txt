file(GLOB HEADERS "include/graphene/track_account/*.hpp")

add_library( graphene_track_account 
             track_account_plugin.cpp
           )

target_link_libraries( graphene_track_account graphene_chain graphene_app )
target_include_directories( graphene_track_account
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

if(MSVC)
  set_source_files_properties( test_plugin.cpp PROPERTIES COMPILE_FLAGS "/bigobj" )
endif(MSVC)

install( TARGETS
   graphene_track_account

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
INSTALL( FILES ${HEADERS} DESTINATION "include/graphene/track_account" )

