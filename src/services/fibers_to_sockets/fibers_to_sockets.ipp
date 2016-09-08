#ifndef SSF_SERVICES_FIBERS_TO_SOCKETS_FIBERS_TO_SOCKETS_IPP_
#define SSF_SERVICES_FIBERS_TO_SOCKETS_FIBERS_TO_SOCKETS_IPP_

#include <ssf/log/log.h>
#include <ssf/network/session_forwarder.h>

#include "common/error/error.h"

namespace ssf {
namespace services {
namespace fibers_to_sockets {

template <typename Demux>
FibersToSockets<Demux>::FibersToSockets(boost::asio::io_service& io_service,
                                        demux& fiber_demux,
                                        local_port_type local_port,
                                        const std::string& ip,
                                        uint16_t remote_port)
    : ssf::BaseService<Demux>::BaseService(io_service, fiber_demux),
      remote_port_(remote_port),
      ip_(ip),
      local_port_(local_port),
      fiber_acceptor_(io_service),
      fiber_(io_service),
      socket_(io_service) {}

template <typename Demux>
void FibersToSockets<Demux>::start(boost::system::error_code& ec) {
  SSF_LOG(kLogInfo) << "microservice[stream_forwarder]: start "
                       "forwarding stream fiber from fiber port " << local_port_
                    << " to TCP <" << ip_ << ":" << remote_port_ << ">";

  endpoint ep(this->get_demux(), local_port_);
  fiber_acceptor_.bind(ep, ec);
  fiber_acceptor_.listen(boost::asio::socket_base::max_connections, ec);

  // Resolve the given address
  boost::asio::ip::tcp::resolver resolver(this->get_io_service());
  boost::asio::ip::tcp::resolver::query query(ip_,
                                              std::to_string(remote_port_));
  boost::asio::ip::tcp::resolver::iterator iterator(
      resolver.resolve(query, ec));

  if (!ec) {
    endpoint_ = *iterator;
    this->StartAcceptFibers();
  }
}

template <typename Demux>
void FibersToSockets<Demux>::stop(boost::system::error_code& ec) {
  SSF_LOG(kLogInfo) << "microservice[stream_forwarder]: stopping";
  ec.assign(::error::success, ::error::get_ssf_category());

  fiber_acceptor_.close();
  manager_.stop_all();
}

template <typename Demux>
uint32_t FibersToSockets<Demux>::service_type_id() {
  return factory_id;
}

template <typename Demux>
void FibersToSockets<Demux>::StartAcceptFibers() {
  SSF_LOG(kLogTrace) << "microservice[stream_forwarder]: accepting new clients";

  fiber_acceptor_.async_accept(
      fiber_, Then(&FibersToSockets::FiberAcceptHandler, this->SelfFromThis()));
}

template <typename Demux>
void FibersToSockets<Demux>::FiberAcceptHandler(
    const boost::system::error_code& ec) {
  SSF_LOG(kLogTrace) << "microservice[stream_forwarder]: accept handler";

  if (!fiber_acceptor_.is_open()) {
    return;
  }

  if (!ec) {
    socket_.async_connect(
        endpoint_,
        Then(&FibersToSockets::SocketConnectHandler, this->SelfFromThis()));
  }
}

template <typename Demux>
void FibersToSockets<Demux>::SocketConnectHandler(
    const boost::system::error_code& ec) {
  SSF_LOG(kLogTrace) << "microservice[stream_forwarder]: connect handler";

  if (!ec) {
    auto session = SessionForwarder<fiber, socket>::create(
        &manager_, std::move(fiber_), std::move(socket_));
    boost::system::error_code e;
    manager_.start(session, e);
  }

  this->StartAcceptFibers();
}

}  // fibers_to_sockets
}  // services
}  // ssf

#endif  // SSF_SERVICES_FIBERS_TO_SOCKETS_FIBERS_TO_SOCKETS_IPP_
