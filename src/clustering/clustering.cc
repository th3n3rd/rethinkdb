#include "clustering/clustering.hpp"

#include "errors.hpp"
#include <boost/bind.hpp>

#include "arch/arch.hpp"
#include "arch/os_signal.hpp"
#include "clustering/administration/http_server.hpp"
#include "clustering/administration/metadata.hpp"
#include "clustering/administration/metadata.hpp"
#include "clustering/administration/reactor_driver.hpp"
#include "http/file_server.hpp"
#include "memcached/clustering.hpp"
#include "mock/dummy_protocol.hpp"
#include "mock/dummy_protocol_parser.hpp"
#include "rpc/connectivity/cluster.hpp"
#include "rpc/connectivity/cluster.hpp"
#include "rpc/connectivity/multiplexer.hpp"
#include "rpc/directory/manager.hpp"
#include "rpc/mailbox/mailbox.hpp"
#include "rpc/semilattice/semilattice_manager.hpp"

struct server_starter_t : public thread_message_t {
    boost::function<void()> fun;
    thread_pool_t *thread_pool;
    void on_thread_switch() {
        coro_t::spawn_sometime(boost::bind(&server_starter_t::run, this));
    }
    void run() {
        fun();
        thread_pool->shutdown();
    }
};

void clustering_main(int port, int contact_port);

int run_server(int argc, char *argv[]) {

    guarantee(argc == 2 || argc == 3, "rethinkdb-clustering expects 1 argument");
    int port = atoi(argv[1]);

    int contact_port = -1;
    if (argc == 3) {
        contact_port = atoi(argv[2]);
    }

    server_starter_t ss;
    thread_pool_t tp(8 /* TODO: Magic constant here. */, false /* TODO: Add --set-affinity option. */);
    ss.thread_pool = &tp;
    ss.fun = boost::bind(&clustering_main, port, contact_port);
    tp.run(&ss);

    return 0;
}

void clustering_main(int port, int contact_port) {
    os_signal_cond_t c;

    connectivity_cluster_t connectivity_cluster; 
    message_multiplexer_t message_multiplexer(&connectivity_cluster);

    message_multiplexer_t::client_t mailbox_manager_client(&message_multiplexer, 'M');
    mailbox_manager_t mailbox_manager(&mailbox_manager_client);
    message_multiplexer_t::client_t::run_t mailbox_manager_client_run(&mailbox_manager_client, &mailbox_manager);

    message_multiplexer_t::client_t semilattice_manager_client(&message_multiplexer, 'S');
    semilattice_manager_t<cluster_semilattice_metadata_t> semilattice_manager_cluster(&semilattice_manager_client, cluster_semilattice_metadata_t(connectivity_cluster.get_me().get_uuid()));
    message_multiplexer_t::client_t::run_t semilattice_manager_client_run(&semilattice_manager_client, &semilattice_manager_cluster);

    message_multiplexer_t::client_t directory_manager_client(&message_multiplexer, 'D');
    directory_readwrite_manager_t<cluster_directory_metadata_t> directory_manager(&directory_manager_client, cluster_directory_metadata_t());
    message_multiplexer_t::client_t::run_t directory_manager_client_run(&directory_manager_client, &directory_manager);

    message_multiplexer_t::run_t message_multiplexer_run(&message_multiplexer);
    connectivity_cluster_t::run_t connectivity_cluster_run(&connectivity_cluster, port, &message_multiplexer_run);

    if (contact_port != -1) {
        connectivity_cluster_run.join(peer_address_t(ip_address_t::us(), contact_port));
    }

    reactor_driver_t<mock::dummy_protocol_t> dummy_reactor_driver(&mailbox_manager,
                                                                  directory_manager.get_root_view()->subview(field_lens(&cluster_directory_metadata_t::dummy_namespaces)), 
                                                                  metadata_field(&cluster_semilattice_metadata_t::dummy_namespaces, semilattice_manager_cluster.get_root_view()));

    reactor_driver_t<memcached_protocol_t> memcached_reactor_driver(&mailbox_manager,
                                                                    directory_manager.get_root_view()->subview(field_lens(&cluster_directory_metadata_t::memcached_namespaces)), 
                                                                    metadata_field(&cluster_semilattice_metadata_t::memcached_namespaces, semilattice_manager_cluster.get_root_view()));

    mock::dummy_protocol_parser_maker_t dummy_parser_maker(&mailbox_manager, 
                                                           metadata_field(&cluster_semilattice_metadata_t::dummy_namespaces, semilattice_manager_cluster.get_root_view()),
                                                           directory_manager.get_root_view()->subview(field_lens(&cluster_directory_metadata_t::dummy_namespaces)));

    memcached_parser_maker_t mc_parser_maker(&mailbox_manager, 
                                             metadata_field(&cluster_semilattice_metadata_t::memcached_namespaces, semilattice_manager_cluster.get_root_view()),
                                             directory_manager.get_root_view()->subview(field_lens(&cluster_directory_metadata_t::memcached_namespaces)));
                                               

    blueprint_http_server_t server(semilattice_manager_cluster.get_root_view(),
                                   connectivity_cluster.get_me().get_uuid(),
                                   port + 1000);
    std::set<std::string> white_list;
    white_list.insert("/cluster.html");
    white_list.insert("/cluster-min.js");
    white_list.insert("/cluster.css");
    white_list.insert("/js/jquery-1.7.1.min.js");
    white_list.insert("/js/handlebars-1.0.0.beta.6.js");
    white_list.insert("/js/underscore-min.js");
    white_list.insert("/js/backbone.js");
    white_list.insert("/js/bootstrap/bootstrap-tab.js");
    white_list.insert("/js/bootstrap/bootstrap-modal.js");
    white_list.insert("/js/bootstrap/bootstrap-alert.js");
    white_list.insert("/js/jquery.sparkline.min.js");
    white_list.insert("/js/flot/jquery.flot.js");
    white_list.insert("/js/flot/jquery.flot.resize.js");
    white_list.insert("/js/jquery.validate.min.js");
    white_list.insert("/js/jquery.hotkeys.js");
    white_list.insert("/js/jquery.form.js");
    white_list.insert("/js/jquery.timeago.js");
    white_list.insert("/js/d3.v2.min.js");
    white_list.insert("/js/date-en-US.js");
    white_list.insert("/favicon.ico");
    white_list.insert("/favicon.ico");
    white_list.insert("/favicon.ico");
    white_list.insert("/favicon.ico");
    white_list.insert("/js/jquery-1.7.1.min.js");
    white_list.insert("/js/handlebars-1.0.0.beta.6.js");
    white_list.insert("/js/underscore-min.js");
    white_list.insert("/js/backbone.js");
    white_list.insert("/js/bootstrap/bootstrap-tab.js");
    white_list.insert("/js/bootstrap/bootstrap-modal.js");
    white_list.insert("/js/bootstrap/bootstrap-alert.js");
    white_list.insert("/js/jquery.sparkline.min.js");
    white_list.insert("/js/flot/jquery.flot.js");
    white_list.insert("/js/flot/jquery.flot.resize.js");
    white_list.insert("/js/jquery.validate.min.js");
    white_list.insert("/js/jquery.hotkeys.js");
    white_list.insert("/js/jquery.form.js");
    white_list.insert("/js/d3.v2.min.js");
    white_list.insert("/js/date-en-US.js");
    white_list.insert("/js/jquery.timeago.js");
    white_list.insert("/favicon.ico");
    http_file_server_t file_server(port + 1001, white_list, "../build/debug/web");

    wait_for_sigint();
}
