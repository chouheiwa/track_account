/*
 * Copyright (c) 2018 Wang Tao (hutaow@hotmail.com), and contributors.
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
#include <graphene/track_account/track_account_plugin.hpp>

#include <graphene/chain/database.hpp>

#include <graphene/chain/market_object.hpp>

#include <graphene/chain/account_object.hpp>
#include <graphene/chain/asset_object.hpp>
#include <graphene/chain/balance_object.hpp>
#include <graphene/chain/chain_property_object.hpp>
#include <graphene/chain/committee_member_object.hpp>
#include <graphene/chain/confidential_object.hpp>
#include <graphene/chain/market_object.hpp>
#include <graphene/chain/operation_history_object.hpp>
#include <graphene/chain/proposal_object.hpp>
#include <graphene/chain/worker_object.hpp>
#include <graphene/chain/witness_object.hpp>

#include <fc/io/fstream.hpp>
#include <iostream>
#include <fstream>

#include <boost/filesystem/path.hpp>
#include <boost/signals2.hpp>
#include <boost/range/algorithm/reverse.hpp>
#include <boost/algorithm/string.hpp>

#define TRACK_ACCOUNT_NAMES "track-account-names"
#define TRACK_ASSET_NAMES "track-asset-names"
#define TRACK_ALL_RELATIONSHIP "track-all-relationship"

using namespace graphene::track_account_plugin;

void track_account_plugin::plugin_set_program_options(
                                                boost::program_options::options_description& command_line_options,
                                                boost::program_options::options_description& config_file_options)
{
    command_line_options.add_options()
    (TRACK_ACCOUNT_NAMES, boost::program_options::value<string>(), "The accounts you want to track")
    (TRACK_ASSET_NAMES, boost::program_options::value<string>(), "The assets you want to track call orders")
    (TRACK_ALL_RELATIONSHIP, boost::program_options::value<bool>(), "Do you need to check out all relation ship")
    ;
    config_file_options.add(command_line_options);
}

std::string track_account_plugin::plugin_name()const
{
    return "track_account_plugin";
}

std::string track_account_plugin::plugin_description()const
{
    return "Track accounts history";
}

void track_account_plugin::plugin_initialize(const boost::program_options::variables_map& options) {
    try {
        ilog("track account plugin: plugin_initialize() begin");
        
        
        if( options.count("plugins") )
        {
            std::vector<string> wanted;
            boost::split(wanted, options.at("plugins").as<std::string>(), [](char c){return c == ' ';});
            
            auto itr = wanted.begin();
            
            bool contain = false;
            
            while (itr != wanted.end()) {
                string name = *itr;
                
                if (name == "account_history") {
                    contain = true;
                    break;
                }
                
                itr++;
            }
            
            FC_ASSERT(contain,"track_account_plugin must used account_history plugin !!! You need to change in config.ini or add plugin in command_line");
            
        }
        
        if (options.count(TRACK_ACCOUNT_NAMES)) {
            auto seeds_str = options.at(TRACK_ACCOUNT_NAMES).as<string>();
            tracked_account_names = fc::json::from_string(seeds_str).as<vector<string>>();
        }
        
        if (options.count(TRACK_ASSET_NAMES)) {
            auto seeds_str = options.at(TRACK_ASSET_NAMES).as<string>();
            tracked_call_orders_asset = fc::json::from_string(seeds_str).as<vector<string>>();
        }
        if (options.count(TRACK_ALL_RELATIONSHIP)) {
            track_all_relationship = options.at(TRACK_ALL_RELATIONSHIP).as<bool>();
            
        }
        
        ilog("TRACK_ACCOUNT_NAMES:${tracked}",("tracked",tracked_account_names));
        ilog("TRACK_ASSET_NAMES:${tracked}",("tracked",tracked_call_orders_asset));
        
        ilog("track account plugin: plugin_initialize() end");
    } FC_LOG_AND_RETHROW()
    
    return;
}

void track_account_plugin::plugin_startup() {
    // 注册事件回调函数
    monitor_signed_block_handler = database().applied_block.connect( [&]( const graphene::chain::signed_block& blk ) {
        monitor_signed_block(blk);
    });
    
    return;
}

void track_account_plugin::plugin_shutdown() {
    // 卸载事件回调函数
    database().applied_block.disconnect(monitor_signed_block_handler);

    return;
}

void track_account_plugin::monitor_signed_block(const graphene::chain::signed_block& blk) {
    auto latency = fc::time_point::now() - blk.timestamp;
    //当小于十秒的时候认为更新到最新区块了
    if (latency < fc::seconds(4) && !already_track_acounts) {
        ilog("Track recive!!!");
        
        already_track_acounts = true;
        
        track_account_creater_register(blk.timestamp);
        
        if (tracked_account_names.size() > 0) {
            const auto& accounts_by_name = database().get_index_type<account_index>().indices().get<by_name>();
            
            auto itr = tracked_account_names.begin();
            
            while (itr != tracked_account_names.end()) {
                const auto&account = accounts_by_name.find(*itr);
                
                if (account != accounts_by_name.end()) {
                    ilog("Track account:${account}",("account",(*account).name));
                    try {
                        track_account_and_write_to_file(*account,blk.timestamp);
                    } catch (...) {
                        elog("Error during track accout:${account}",("account",(*account).name));
                    }
                    
                    
                }else {
                    elog("Track account:${account} not found!!!",("account",*itr));
                }
                itr++;
            }
        }
        
        if (tracked_call_orders_asset.size() > 0) {
            const auto& assets_by_symbol = database().get_index_type<asset_index>().indices().get<by_symbol>();
            
            auto itr_asset = tracked_call_orders_asset.begin();
            
            while (itr_asset != tracked_call_orders_asset.end()) {
                string symbol_or_id = *itr_asset;
                ilog("tracked_call_orders_asset begin:${asset}",("asset",symbol_or_id));
                try {
                    if( !symbol_or_id.empty() && std::isdigit(symbol_or_id[0]) )
                    {
                        auto ptr = database().find(variant(symbol_or_id).as<asset_id_type>());
                        asset_object asset = *ptr;
                        track_call_orders_and_write_to_file(asset,blk.timestamp);
                    }else {
                        auto ptr = assets_by_symbol.find(symbol_or_id);
                        asset_object asset = *ptr;
                        track_call_orders_and_write_to_file(asset,blk.timestamp);
                    }
                    ilog("tracked_call_orders_asset end:${asset}",("asset",symbol_or_id));
                } catch (...) {
                    elog("tracked_call_orders_asset error when get asset:${asset}",("asset",symbol_or_id));
                }
                itr_asset++;
            }
        }
        ilog("Track end!!!Program will end");
        
        exit(0);
    }
}

void track_account_plugin::track_account_creater_register(fc::time_point track_block_time) {
    if (!track_all_relationship) return;
    
    const auto& db = database();
    
    ofstream out(fc::string(track_block_time) + "_账户关系" + ".csv",ios::app);
    
    long long value = 6;
    
    while (true) {
        try {
            const auto&account = account_id_type(value)(db);
            string registers;
            string registers_id;
            string reffer;
            string reffer_id;
            
            if (account.is_lifetime_member()) {
                const auto& stats = account.statistics(db);
                const account_transaction_history_object* node = &stats.most_recent_op(db);
                while(node && node->operation_id.instance.value > 0) {
                    if( node->next == account_transaction_history_id_type() ){
                        operation_history_object operation = node->operation_id(db);
                        
                        int which = operation.op.which();
                        
                        if (which == 5) {
                            account_create_operation op = operation.op.get<account_create_operation>();
                            
                            if (op.name != account.name) {
                                registers = "";
                                registers_id = "";
                                
                                reffer = "";
                                reffer_id = "";
                                break;
                            }
                            
                            const auto registrar = op.registrar(db);
                            const auto reffers = op.referrer(db);
                            
                            registers = registrar.name;
                            registers_id = fc::json::to_string(registrar.id);
                            
                            reffer = reffers.name;
                            reffer_id = fc::json::to_string(reffers.id);
                            
                        }else {
                            registers = "";
                            registers_id = "";
                            
                            reffer = "";
                            reffer_id = "";
                        }
                        break;
                    }else node = &node->next(db);
                }
            }else {
                const auto registrar = account.registrar(db);
                const auto reffers = account.referrer(db);
                
                registers = registrar.name;
                registers_id = fc::json::to_string(registrar.id);
                
                reffer = reffers.name;
                reffer_id = fc::json::to_string(reffers.id);
            }
            out<<account.name<<",";
            out<<fc::json::to_string(account.id)<<",";
            out<<registers<<",";
            out<<registers_id<<",";
            out<<reffer<<",";
            out<<reffer_id<<endl;
        } catch (fc::exception &ex) {
            elog("Catch exception:${ex}",("ex",ex));
            break;
        }
        value ++;
    }
    out.close();
    
}


void track_account_plugin::track_account_and_write_to_file(account_object account,fc::time_point track_block_time) {
    
    const auto& db = database();
    const auto& stats = account.statistics(db);
    
    ilog("Track_brgin ${ip}",("ip",stats.most_recent_op));
    if( stats.most_recent_op == account_transaction_history_id_type() ) return;
    ofstream out(account.name + "_" + fc::json::to_string(track_block_time) + ".csv",ios::app);
    
    out<<"Date,Operation,Fee,Amount,From,To"<<endl;
    
    const account_transaction_history_object* node = &stats.most_recent_op(db);
    operation_history_id_type start = node->operation_id;
    
    while(node && node->operation_id.instance.value > 0)
    {
        if( node->operation_id.instance.value <= start.instance.value ) {
            operation_history_object operation = node->operation_id(db);
            
            signed_block signed_block = *db.fetch_block_by_number(operation.block_num);
            //时间
            out<<signed_block.timestamp.to_iso_string() + ",";
            
            int which = operation.op.which();
            
            if (which == 0) {
                //类型
                out<<"Transfer,";
                transfer_operation op = operation.op.get<transfer_operation>();
                const auto fee_object = op.fee.asset_id(db);
                const auto &amount_object = op.amount.asset_id(db);
                //手续费
                out<<fee_object.amount_to_pretty_string(op.fee) + ",";
                //数量
                out<<amount_object.amount_to_pretty_string(op.amount) + ",";
                if (account.id == op.from) {
                    //转出账户
                    out<<account.name + ",";
                    
                    const auto &another_account = op.to(db);
                    
                    out<<another_account.name;
                }else {
                    const auto &another_account = op.from(db);
                    
                    out<<another_account.name + ",";
                    
                    out<<account.name;
                }
            }else if (which == 1) {
                out<<"limit_order_create_operation,";
                limit_order_create_operation op = operation.op.get<limit_order_create_operation>();
                const auto fee_object = op.fee.asset_id(db);
                const auto &sell_object = op.amount_to_sell.asset_id(db);
                const auto &receive_object = op.min_to_receive.asset_id(db);
                //手续费
                out<<fee_object.amount_to_pretty_string(op.fee) + ",";
                //卖出数量
                out<<"sell:" + sell_object.amount_to_pretty_string(op.amount_to_sell) + ",";
                //最少收到
                out<<"receive:" + receive_object.amount_to_pretty_string(op.min_to_receive) + ",";
                //挂单者
                out<<"name:" + account.name;
            }else if (which == 2) {
                out<<"limit_order_cancel_operation,";
                limit_order_cancel_operation op = operation.op.get<limit_order_cancel_operation>();
                const auto fee_object = op.fee.asset_id(db);
                //手续费
                out<<fee_object.amount_to_pretty_string(op.fee) + ",";
                //撤单单号
                out<<"order_id:" + fc::json::to_string(op.order) + ",";
                //撤单者
                out<<account.name;
                
            }else if (which == 4) {
                out<<"fill_order_operation,";
                fill_order_operation op = operation.op.get<fill_order_operation>();
                const auto fee_object = op.fee.asset_id(db);
                const auto &pay_object = op.pays.asset_id(db);
                const auto &receive_object = op.receives.asset_id(db);
                //手续费
                out<<fee_object.amount_to_pretty_string(op.fee) + ",";
                //支出
                out<<"Pay:" + pay_object.amount_to_pretty_string(op.pays) + ",";
                //收入
                out<<"Receive:" + receive_object.amount_to_pretty_string(op.receives) + ",";
                
                out<<account.name;
                
            }else if (which == 5) {
                out<<"account_create_operation";
            }else {
                out<<"unknown operation";
            }
            
            
            out << endl;
        }
            
        if( node->next == account_transaction_history_id_type() )
            node = nullptr;
        else node = &node->next(db);
    }
    
    ilog("Track_end");
    
    out.close();
}

void track_account_plugin::track_call_orders_and_write_to_file(asset_object mia,fc::time_point track_block_time) {
    const auto& _db = database();
    
    const auto& call_index = _db.get_index_type<call_order_index>().indices().get<by_price>();
    
    ofstream out(mia.symbol + "_" + fc::json::to_string(track_block_time) + ".csv",ios::app);
    
    out<<"Borrower,Collateral_Amount,Collateral_Symbol,Debt_Amount,Debt_Symbol"<<endl;
    
    price index_price = price::min(mia.bitasset_data(_db).options.short_backing_asset, mia.get_id());
    
    vector< call_order_object> result;
    auto itr_min = call_index.lower_bound(index_price.min());
    auto itr_max = call_index.lower_bound(index_price.max());
    while( itr_min != itr_max )
    {
        call_order_object call = *itr_min;
        
        const auto collateral = call.get_collateral();
        const auto &collateral_asset = collateral.asset_id(_db);
        const auto debt = call.get_debt();
        const auto &debt_asset = debt.asset_id(_db);
        const auto &borrower = call.borrower(_db);
        
        out<<borrower.name + ",";
        out<<collateral_asset.amount_to_string(collateral) + ",";
        out<<collateral_asset.symbol + ",";
        out<<debt_asset.amount_to_string(debt) + ",";
        out<<debt_asset.symbol;
        out<<endl;
        ++itr_min;
    }
    
    out.close();
}

vector<operation_history_object> track_account_plugin::get_account_history_operations( account_id_type account,
                                                                             int operation_id,
                                                                             operation_history_id_type start,
                                                                             operation_history_id_type stop,
                                                                             unsigned limit)
{
    const auto& db = database();
    vector<operation_history_object> result;
    const auto& stats = account(db).statistics(db);
    if( stats.most_recent_op == account_transaction_history_id_type() ) return result;
    const account_transaction_history_object* node = &stats.most_recent_op(db);
    if( start == operation_history_id_type() )
        start = node->operation_id;
    
    while(node && node->operation_id.instance.value > stop.instance.value && result.size() < limit)
    {
        if( node->operation_id.instance.value <= start.instance.value ) {
            
            if(node->operation_id(db).op.which() == operation_id)
                result.push_back( node->operation_id(db) );
        }
        if( node->next == account_transaction_history_id_type() )
            node = nullptr;
        else node = &node->next(db);
    }
    if( stop.instance.value == 0 && result.size() < limit ) {
        const account_transaction_history_object head = account_transaction_history_id_type()(db);
        if( head.account == account && head.operation_id(db).op.which() == operation_id )
            result.push_back(head.operation_id(db));
    }
    return result;
}

vector<order_history_object> track_account_plugin::get_fill_orders( asset_id_type a, asset_id_type b,object_id_type order_id,share_type sell_amount) {
    const auto& db = database();
    if( a > b ) std::swap(a,b);
    const auto& history_idx = db.get_index_type<graphene::market_history::history_index>().indices().get<by_key>();
    history_key hkey;
    hkey.base = a;
    hkey.quote = b;
    hkey.sequence = std::numeric_limits<int64_t>::min();
    
    auto itr = history_idx.lower_bound( hkey );
    ++itr;
    vector<order_history_object> result;
    while( itr != history_idx.end()  && sell_amount > 0)
    {
        if( itr->key.base != a || itr->key.quote != b ) break;
        if (itr->op.order_id.instance() != order_id.instance()) {
            ++itr;
            ++itr;
            continue;
        }
        
        sell_amount -= itr->op.pays.amount;
        --itr;
        result.push_back(*itr);
        ++itr;
        result.push_back( *itr );
        ++itr;
        ++itr;
    }
    
    return result;
}

//vector<order_history_object> track_account_plugin::get_fill_order_history( asset_id_type a, asset_id_type b, uint32_t limit  )
//{
//    const auto& db = database();
//    if( a > b ) std::swap(a,b);
//    const auto& history_idx = db.get_index_type<history_index>().indices().get<by_key>();
//    history_key hkey;
//    hkey.base = a;
//    hkey.quote = b;
//    hkey.sequence = std::numeric_limits<int64_t>::min();
//
//    uint32_t count = 0;
//    auto itr = history_idx.lower_bound( hkey )
//}
