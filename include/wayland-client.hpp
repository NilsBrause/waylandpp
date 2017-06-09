/*
 * Copyright (c) 2017 Philipp Kerling
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer. 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WAYLAND_CLIENT_HPP
#define WAYLAND_CLIENT_HPP

#include <wayland-client-core.hpp>
#include <wayland-client-protocol.hpp>

namespace wayland
{
  /** \brief Bind a global object to a provided protocol wrapper if the interface
   * matches the provided one
   * 
   * This is useful for \ref registry_t::on_global handlers since they
   * do not need to do error-prone name-based matching, but can instead
   * just pass the protocol wrapper they would like to be bound and have
   * the name-check done here automatically.
   * 
   * The version that is bound is the minimum of the version parameter given
   * here (the version that the compositor has) and the version that the
   * client-side interface supports to avoid getting an unsupported version
   * error on either side. You can check the version that was actually bound
   * afterwards with \ref proxy_t::get_version
   * 
   * An exception will be thrown if target is a raw \ref proxy_t without associated
   * interface.
   * 
   * \param registry registry to use for binding
   * \param target proxy object that should be bound if the type matches
   * \param name global interface name as received by \ref registry_t::on_global
   * \param interface string interface name as received by \ref registry_t::on_global
   * \param version announced interface version as received by \ref registry_t::on_global
   * \return whether the requested interface was bound to the target
   */
  bool registry_try_bind(registry_t registry, proxy_t &target, uint32_t name, std::string interface, uint32_t version);
  /** \brief Bind a global object to a provided protocol wrapper if the interface
   * matches one of the provided ones
   * 
   * This is identical to the single-target registry_try_bind except that a list
   * of multiple protocol wrappers can be passed of which each one is tried
   * in order.
   * 
   * \param registry registry to use for binding
   * \param targest proxy objects of which one should be bound if the type matches
   * \param name global interface name as received by \ref registry_t::on_global
   * \param interface string interface name as received by \ref registry_t::on_global
   * \param version announced interface version as received by \ref registry_t::on_global
   * \return whether any target protocol wrapper was bound
   */
  bool registry_try_bind(registry_t registry, std::initializer_list<std::reference_wrapper<proxy_t>> targets, uint32_t name, std::string interface, uint32_t version);
}

#endif