## 插件介绍 Plugin Introduce
  这个插件是比特股见证节点的一个查找对账插件，目的是为个人满足追踪区块链上某一个指定账户全部历史记录，如果用cli_wallet查询时间太久，因此这个插件是采用直接解析节点账户记录，并生成csv文件
  
  This plugin is one plugin of bitshares that can help you to get one or more accout's all account history. I made this plugin because of if only use cli_wallet that get account history may take a long time.This plugin can generate csv file of account.
   
  
  同时这个插件可以查询区块链全部账户上下级信息(如谁注册了这个账户，当时的推荐人是谁)
  
  This plugin can also get all account's relationship like who created this account and who was the first reffer.
## 插件编译方法 Complie plugin
  插件需要和比特股见证节点一同被编译打包，因此接下来我们的默认情况是您已经下载bitshares源代码，并熟悉其他编译流程
```
# 首先需要先进入到bitshares源代码目录
# First you need to cd to bitshares source code
# 接下来添加git 子仓库
# Then add submodule to bitshares
git submodule add "https://github.com/chouheiwa/track_account.git" libraries/plugins/track_account
```  
   接下来需要在libraries/plugins/CMakeLists.txt中增加一行
   
   Next you need to add a line to libraries/plugins/CMakeLists.txt
```
add_subdirectory( track_account )
```
   最后一步，需要在programs/witness_node中CMakeLists.txt增加相关依赖 并且在该文件夹下main.m中导入插件
   
   Final step,you need to add dependency to programs/witness_node/CMakeLists.txt and change main.m to import the plugin

```
# 修改CMakeLists.txt 文件中
# Change CMakeLists.txt
target_link_libraries( witness_node PRIVATE ...)
# 在上面展示的地方的 PRIVATE 后增加 graphene_track_account
# Add graphene_track_account behind the 'PRIVATE'

# 修改main.m
# Change main.m
# 导入头文件
# import Header file

#include <graphene/track_account/track_account_plugin.hpp>

# 在当前文件中搜索"node->register_plugin"
# search words "node->register_plugin" in current file 
# 在搜索到位置上方增加一行
# Add a line at the position of search

auto track_plug = node->register_plugin<track_account_plugin::track_account_plugin>();
```
这一步后插件就会被导入到节点中，等待编译完成后可执行文件中便可启用相关插件
## 插件启用方法 Plugin usage
   插件可以通过命令行参数启动，或者通过配置config.ini，这里我只介绍配置config.ini
   
   Plugin can started by command line params or config.ini,here I only to show how to use in config.ini
   
```
# 这个插件有一个依赖插件 account_history，因此启动这个插件的时候必须同时启动account_history
# This plugin has a dependency plugin 'account_history'.So both two plugins need to start.
plugins = account_history track_account_plugin

# 如果你希望查询一个名字叫wsf123456账户的全部历史记录，你可以在下面的参数中，以json数组的形式配置，并取消注释
# The accounts you want to track of json string array
track-account-names = ["wsf123456"]
# 如果你希望查看某个智能资产的全部抵押借入信息，传入相关币种名称的json数组，并取消注释
# The assets you want to track call orders of json string array
# track-asset-names =
# 如果希望查询账户关系图，此项填true
# Do you need to check out all relation ship
track-all-relationship = false

```
最后是标准启动节点程序,程序运行完成后，相关文件会在执行见证节点目录下生成

Finaly is to run noral start,target file will be at the run path

## 其余相关说明

这个插件在文件生成结束后会自动停止见证节点（如果节点较大，启动时间可能耗时较长）

目前存在问题:

1. 目前这个插件的编译启动环境只能是DEBUG
    
    From now on this plugin can only make of DEBUG.
