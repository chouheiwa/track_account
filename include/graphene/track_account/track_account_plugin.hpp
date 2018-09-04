/*
 * Copyright (c) 2015 Cryptonomex, Inc., and contributors.
 *
 * The MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#pragma once

#include <graphene/app/plugin.hpp>
#include <graphene/chain/database.hpp>

#include <graphene/chain/operation_history_object.hpp>
#include <graphene/market_history/market_history_plugin.hpp>

#include <fc/thread/future.hpp>

using namespace graphene::chain;
using namespace graphene::market_history;
using namespace std;

namespace graphene { namespace track_account_plugin {
#define MONITOR_ACTION_TYPE_ALL (0xFFFFFFFF)
    
    class track_account_plugin : public graphene::app::plugin {
    public:
        // 插件名称
        std::string plugin_name()const override;
        
        // 插件描述信息
        std::string plugin_description()const override;
        
        // 配置参数注册
        virtual void plugin_set_program_options(
                                                boost::program_options::options_description &command_line_options,
                                                boost::program_options::options_description &config_file_options
                                                ) override;
        
        // 插件初始化
        virtual void plugin_initialize( const boost::program_options::variables_map& options ) override;
        
        // 启动插件
        virtual void plugin_startup() override;
        
        // 停止插件
        virtual void plugin_shutdown() override;
        
    private:
        // 事件注册句柄
        boost::signals2::connection monitor_signed_block_handler;
        // 查找账户名
        vector<string> tracked_account_names;
        // 查找资产信息
        vector<string> tracked_call_orders_asset;
        
        bool already_track_acounts = false;
        
        // 查找指定账户并写入
        void track_account_and_write_to_file(account_object account,fc::time_point track_block_time);
        
        void track_call_orders_and_write_to_file(asset_object asset,fc::time_point track_block_time);
        
        // 生成区块事件回调函数
        void monitor_signed_block( const graphene::chain::signed_block& b);
        
        vector<operation_history_object> get_account_history_operations( account_id_type account,
                                                                                     int operation_id,
                                                                                     operation_history_id_type start,
                                                                                     operation_history_id_type stop,
                                                                                     unsigned limit);
        
        vector<order_history_object> get_fill_orders( asset_id_type a, asset_id_type b,object_id_type order_id,share_type sell_amount);
    };
} } // graphene::monitor_plugin
