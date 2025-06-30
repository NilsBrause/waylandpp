/*
 * Copyright (c) 2022, Nils Christopher Brause
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

#ifndef WAYLAND_SERVER_HPP
#define WAYLAND_SERVER_HPP

#include <atomic>
#include <functional>
#include <list>
#include <memory>
#include <string>

#include <wayland-server-core.h>
#include <wayland-util.hpp>

/** \file */

namespace wayland
{
  namespace server
  {
    namespace detail
    {
      struct listener_t
      {
        wl_listener listener = { { nullptr, nullptr }, nullptr };
        void *user = nullptr;
      };
    }

    /** \brief Type for functions that handle log messages
     *
     * Log message is the first argument
     */
    using log_handler = std::function<void(std::string)>;

    /** \brief Set C library log handler
     *
     * The C library sometimes logs important information such as protocol
     * error messages, by default to the standard output. This can be used
     * to set an alternate function that will receive those messages.
     *
     * \param handler function that should be called for C library log messages
     */
    void set_log_handler(const log_handler& handler);

    class client_t;
    class global_base_t;
    template <class resource> class global_t;
    class event_loop_t;
    class event_source_t;

    class display_t
    {
    private:
      struct data_t
      {
        std::function<void()> destroy;
        std::function<void(client_t&)> client_created;
        detail::listener_t destroy_listener;
        detail::listener_t client_created_listener;
        std::function<bool(client_t, global_base_t)> filter_func;
        wayland::detail::any user_data;
        std::atomic<unsigned int> counter{1};
      };

      wl_display *display = nullptr;
      data_t *data = nullptr;

      static void destroy_func(wl_listener *listener, void *data);
      static void client_created_func(wl_listener *listener, void *cl);
      static data_t *wl_display_get_user_data(wl_display *display);
      static bool c_filter_func(const wl_client *client, const wl_global *global, void *data);

    protected:
      display_t(wl_display *c);
      void init();
      void fini();

      friend class client_t;

    public:
      /** Create Wayland display object.
       *
       * This creates the display object.
       */
      display_t();

      /** Destroy Wayland display object.
       *
       * This function emits the wl_display destroy signal, releases
       * all the sockets added to this display, free's all the globals associated
       * with this display, free's memory of additional shared memory formats and
       * destroy the display object.
       *
       * \sa display_t::on_destroy
       */
      ~display_t();
      display_t(const display_t& d);
      display_t(display_t&& d) noexcept;
      display_t &operator=(const display_t& d);
      display_t &operator=(display_t&& d) noexcept;
      bool operator==(const display_t& d) const;
      wl_display *c_ptr() const;
      wayland::detail::any &user_data();

      /** Retruns the event loop
       *
       * This function return the event loop associated with the display.
       * It can be used to manually dispatch events instead of using run().
       */
      event_loop_t get_event_loop() const;

      /** Add a socket to Wayland display for the clients to connect.
       *
       * \param name Name of the Unix socket.
       * \return 0 if success. -1 if failed.
       *
       * This adds a Unix socket to Wayland display which can be used by clients to
       * connect to Wayland display.
       *
       * If "" is passed as name, then it would look for WAYLAND_DISPLAY env
       * variable for the socket name. If WAYLAND_DISPLAY is not set, then default
       * wayland-0 is used.
       *
       * The Unix socket will be created in the directory pointed to by environment
       * variable XDG_RUNTIME_DIR. If XDG_RUNTIME_DIR is not set, then this function
       * fails and returns -1.
       *
       * The length of socket path, i.e., the path set in XDG_RUNTIME_DIR and the
       * socket name, must not exceed the maximum length of a Unix socket path.
       * The function also fails if the user do not have write permission in the
       * XDG_RUNTIME_DIR path or if the socket name is already in use.
       */
      int add_socket(const std::string& name) const;

      /** Add the default socket to Wayland display for the clients to connect.
       *
       * This uses add_socket() to add the default socket 'wayland-0' to the
       * Wayland display, incrementing the number if it already exists.
       */
      std::string add_socket_auto() const;

      /**  Add a socket with an existing fd to Wayland display for the clients to connect.
       *
       * \param sock_fd The existing socket file descriptor to be used
       * \return 0 if success. -1 if failed.
       *
       * The existing socket fd must already be created, opened, and locked.
       * The fd must be properly set to CLOEXEC and bound to a socket file
       * with both bind() and listen() already called.
       */
      int add_socket_fd(int sock_fd) const;

      /** Stops the event dispatching loop
       *
       * This funtion terminates the loop in the function run() and thus stops
       * further dispatching of events.
       */
      void terminate() const;

      /** Runs the internal event dispatching loop
       * This function is the internal event dispatching loop and can be used
       * in case the events shall not be manually dispatched using
       * get_event_loop(). This function returns when terminate() is called.
       */
      void run() const;

      /** Sends buffered requests to the clients.
       *
       * Requests that are sent to a client are buffered. This flushes the
       * buffers of all client connections and sends pendings requests to
       * the clients. This is an integral part of every event loop.
       */
      void flush_clients() const;

      /** Get the current serial number
       *
       * This function returns the most recent serial number, but does not
       * increment it.
       */
      uint32_t get_serial() const;

      /** Get the next serial number
       *
       * This function increments the display serial number and returns the
       * new value.
       */
      uint32_t next_serial() const;
      std::function<void()> &on_destroy();

      /** Registers a listener for the client connection signal.
       *  When a new client object is created, \a listener will
       *  be notified, carrying the new client_t object.
       */
      std::function<void(client_t&)> &on_client_created();

      /** Create client from a file descriptor
       *
       * Normally, clients connect to the socket created with add_socket()
       * or add_socket_auto(). Here, a new client can be created with an
       * existing connection using the associated file descriptor.
       */
      client_t client_create(int fd);

      /** Get the list of currently connected clients
       *
       * \param display The display object
       *
       * This function returns the list of clients currently
       * connected to the display.
       */
      std::list<client_t> get_client_list() const;

      /** Set a filter function for global objects
       *
       * \param filter  The global filter funtion.
       *
       * Set a filter for the display to advertise or hide global objects
       * to clients.
       * The set filter will be used during global advertisment to
       * determine whether a global object should be advertised to a
       * given client, and during global binding to determine whether
       * a given client should be allowed to bind to a global.
       *
       * Clients that try to bind to a global that was filtered out will
       * have an error raised.
       */
      void set_global_filter(const std::function<bool(client_t, global_base_t)>& filter);

      /** Sets the default maximum size for connection buffers of new clients.
       *  This function sets the default size of the internal connection buffers for new clients. It doesn't change the buffer size for existing clients.
       *
       * The connection buffer size of an existing client can be adjusted using @see client_t::set_max_buffer_size().
       *
       * The actual size of the connection buffers is a power of two, the requested max_buffer_size is therefore rounded up to the nearest power of two value.
       *
       * The minimum buffer size is 4096.
       */
      void set_default_max_buffer_size(size_t max_buffer_size);
    };

    class resource_t;

    class client_t
    {
    private:
      struct data_t
      {
        wl_client *client = nullptr;
        std::function<void()> destroy;
        detail::listener_t destroy_listener;
        wayland::detail::any user_data;
        std::atomic<unsigned int> counter{1};
        bool destroyed = false;
        std::function<void()> destroy_late;
        detail::listener_t destroy_late_listener;
        std::function<void(resource_t&)> resource_created;
        detail::listener_t resource_created_listener;
      };

      wl_client *client = nullptr;
      data_t *data = nullptr;

      static void destroy_func(wl_listener *listener, void *data);
      static wl_iterator_result resource_iterator(wl_resource *resource, void *data);
      static void destroy_late_func(wl_listener *listener, void *data);
      static void resource_created_func(wl_listener *listener, void *data);
      static void user_data_destroy_func(void *data);

    protected:
      client_t(wl_client *c);
      void init();

      friend class display_t;
      friend class resource_t;
      template <class resource> friend class global_t;

    public:
      /** Create a client for the given file descriptor
       *
       * \param display The display object
       * \param fd The file descriptor for the socket to the client
       * \return The new client object or NULL on failure.
       *
       * Given a file descriptor corresponding to one end of a socket, this
       * function will create a wl_client struct and add the new client to
       * the compositors client list.  At that point, the client is
       * initialized and ready to run, as if the client had connected to the
       * servers listening socket.  When the client eventually sends
       * requests to the compositor, the wl_client argument to the request
       * handler will be the wl_client returned from this function.
       *
       * The other end of the socket can be passed to
       * wl_display_connect_to_fd() on the client side or used with the
       * WAYLAND_SOCKET environment variable on the client side.
       *
       * Listeners added with wl_display_add_client_created_listener() will
       * be notified by this function after the client is fully constructed.
       *
       * On failure this function sets errno accordingly and returns NULL.
       */
      client_t(display_t &display, int fd);
      client_t() = delete;
      ~client_t();
      client_t(const client_t &c);
      client_t(client_t &&c) noexcept;
      client_t &operator=(const client_t& c);
      client_t &operator=(client_t&& c) noexcept;
      bool operator==(const client_t &c) const;
      wl_client *c_ptr() const;
      wayland::detail::any &user_data();

      /** Flush pending events to the client
       *
       * Events sent to clients are queued in a buffer and written to the
       * socket later - typically when the compositor has handled all
       * requests and goes back to block in the event loop.  This function
       * flushes all queued up events for a client immediately.
       */
      void flush() const;

      /** Return Unix credentials for the client
       *
       * \param pid Returns the process ID
       * \param uid Returns the user ID
       * \param gid Returns the group ID
       *
       * This function returns the process ID, the user ID and the group ID
       * for the given client.  The credentials come from getsockopt() with
       * SO_PEERCRED, on the client socket fd.
       *
       * Be aware that for clients that a compositor forks and execs and
       * then connects using socketpair(), this function will return the
       * credentials for the compositor.  The credentials for the socketpair
       * are set at creation time in the compositor.
       */
      void get_credentials(pid_t &pid, uid_t &uid, gid_t &gid) const;

      /** Get the file descriptor for the client
       *
       * \return The file descriptor to use for the connection
       *
       * This function returns the file descriptor for the given client.
       *
       * Be sure to use the file descriptor from the client for inspection only.
       * If the caller does anything to the file descriptor that changes its state,
       * it will likely cause problems.
       *
       * See also client_t::get_credentials().
       * It is recommended that you evaluate whether client_t::get_credentials()
       * can be applied to your use case instead of this function.
       *
       * If you would like to distinguish just between the client and the compositor
       * itself from the client's request, it can be done by getting the client
       * credentials and by checking the PID of the client and the compositor's PID.
       * Regarding the case in which the socketpair() is being used, you need to be
       * careful. Please note the documentation for client_t::get_credentials().
       *
       * This function can be used for a compositor to validate a request from
       * a client if there are additional information provided from the client's
       * file descriptor. For instance, suppose you can get the security contexts
       * from the client's file descriptor. The compositor can validate the client's
       * request with the contexts and make a decision whether it permits or deny it.
       */
      int get_fd() const;

      /** Add a callback to be called at the beginning of client destruction.
       *
       * The callback provided will be called when client destroy has begun,
       * before any of that client's resources have been destroyed.
       */
      std::function<void()> &on_destroy();

      /** Look up an object in the client name space
       *
       * \param id The object id
       * \return The object or NULL if there is not object for the given ID
       *
       * This looks up an object in the client object name space by its
       * object ID.
       */
      resource_t get_object(uint32_t id);

      /** Post "not enough memory" error to the client
       *
       * If the compositor has not enough memory to fulfill a certail request
       * of the client, this function can be called to notify the client of
       * this circumstance.
       */
      void post_no_memory() const;

      /** Report an internal server error
       *
       * \param msg A message string
       *
       * Report an unspecified internal implementation error and disconnect
       * the client.
       */
      void post_implementation_error(std::string const& msg) const;

      /** Report an internal server error
       *
       * \param msg A printf-style format string
       * \param args Format string arguments
       *
       * Report an unspecified internal implementation error and disconnect
       * the client.
       */
      template <typename...types>
      void post_implementation_error(std::string const& msg, types&&... args)
      {
        wl_client_post_implementation_error(c_ptr(), msg.c_str(), std::forward<types...>(args...));
      }

      /** Get the display object for the given client
       *
       * \return The display object the client is associated with.
       */
      display_t get_display() const;

      /** Adjust the maximum size of the client connection buffers
       *
       * \param max_buffer_size The maximum size of the connection buffers
       *
       * The actual size of the connection buffers is a power of two, the requested
       * max_buffer_size is therefore rounded up to the nearest power of two value.
       *
       * Lowering the maximum size may not take effect immediately if the current
       *  content of the buffer does not fit within the new size limit.
       *
       * The minimum buffer size is 4096. The default buffers size can be set using
       * @see display_t::set_default_max_buffer_size().
       */
      void set_max_buffer_size(size_t max_buffer_size);

      /** Get a list of the clients resources.
       *
       * \return A list of resources used by the clienzt
       */
      std::list<resource_t> get_resource_list() const;

      /** Add a callback to be called at the end of wl_client destruction.
       *
       * The callback provided will be called when wl_client destroy is nearly complete,
       * after all of that client's resources have been destroyed.
       */
      std::function<void()> &on_destroy_late();

      /** Add a callback for the client's resource creation signal
       *
       * When a new resource is created for this client the listener will be notified,
       * carrying the new resource as its argument.
       */
      std::function<void(resource_t&)> &on_resource_created();
    };

    class resource_t
    {
    protected:
      // base class for event listener storage.
      struct events_base_t
      {
        events_base_t() = default;
        events_base_t(const events_base_t& e) = default;
        events_base_t(events_base_t&& e) = default;
        events_base_t& operator=(const events_base_t& e) = default;
        events_base_t& operator=(events_base_t&& e) = default;
        virtual ~events_base_t() = default;
      };

    private:
      struct data_t
      {
        std::shared_ptr<events_base_t> events;
        std::function<void()> destroy;
        detail::listener_t destroy_listener;
        wayland::detail::any user_data;
        std::atomic<unsigned int> counter{1};
        bool destroyed = false;
      };

      wl_resource *resource = nullptr;
      data_t *data = nullptr;

      static void destroy_func(wl_listener *listener, void *data);
      static int c_dispatcher(const void *implementation, void *target,
                              uint32_t opcode, const wl_message *message,
                              wl_argument *args);
      static int dummy_dispatcher(int opcode, const std::vector<wayland::detail::any>& args, const std::shared_ptr<resource_t::events_base_t>& events);

    protected:
      // Interface desctiption filled in by the each interface class
      static constexpr const wl_interface *interface = nullptr;

      /*
        Sets the dispatcher and its user data. User data must be an
        instance of a class derived from events_base_t, allocated with
        new. Will automatically be deleted upon destruction.
      */
      void set_events(const std::shared_ptr<events_base_t>& events,
                      int(*dispatcher)(int, const std::vector<wayland::detail::any>&, const std::shared_ptr<resource_t::events_base_t>&));

      // Retrieve the perviously set user data
      std::shared_ptr<events_base_t> get_events() const;

      void post_event_array(uint32_t opcode, const std::vector<wayland::detail::argument_t>& v) const;
      void queue_event_array(uint32_t opcode, const std::vector<wayland::detail::argument_t>& v) const;

      template <typename...T>
      void post_event(uint32_t opcode, T...args) const
      {
        std::vector<wayland::detail::argument_t> v = { wayland::detail::argument_t(args)... };
        if(c_ptr())
          post_event_array(opcode, v);
      }

      template <typename...T>
      void queue_event(uint32_t opcode, T...args) const
      {
        std::vector<wayland::detail::argument_t> v = { wayland::detail::argument_t(args)... };
        if(c_ptr())
          queue_event_array(opcode, v);
      }

      template <typename...T>
      void send_event(bool post, uint32_t opcode, T...args) const
      {
        if(post)
          post_event(opcode, args...);
        else
          queue_event(opcode, args...);
      }

      void post_error(uint32_t code, const std::string& msg) const;

      resource_t(wl_resource *c);
      void init();
      void fini();

      friend class client_t;

    public:
      resource_t() = default;

      /** Create a new resource object
       *
       * \param client The client owner of the new resource.
       * \param interface The interface of the new resource.
       * \param version The version of the new resource.
       * \param id The id of the new resource. If 0, an available id will be used.
       *
       * Listeners added with \a client_t::on_resource_created will be
       * notified at the end of this function.
       */
      resource_t(const client_t& client, const wl_interface *interface, int version, uint32_t id);
      ~resource_t();
      resource_t(const resource_t &r);
      resource_t(resource_t &&r) noexcept;
      resource_t &operator=(const resource_t& r);
      resource_t &operator=(resource_t&& r) noexcept;
      bool operator==(const resource_t& r) const;
      operator bool() const;
      wl_resource *c_ptr() const;
      wayland::detail::any &user_data();

      /** \brief Check whether this wrapper actually wraps an object
       *  \return true if there is an underlying object, false if this wrapper is
       *          empty
       */
      bool proxy_has_object() const;

      /** Post "not enough memory" error to the client
       *
       * If the compositor has not enough memory to fulfill a certail request
       * of the client, this function can be called to notify the client of
       * this circumstance.
       */
      void post_no_memory() const;

      /** Get the internal ID of the resource
       *
       * \return the internal ID of the resource
       */
      uint32_t get_id() const;

      /** Get the associated client
       *
       * \return the client that owns the resource.
       */
      client_t get_client() const;

      /** Get interface version
       *
       * \return Interface version this resource has been constructed with.
       */
      unsigned int get_version() const;

      /** Retrieve the interface name (class) of a resource object.
       *
       * \return Interface name of the resource object.
       */
      std::string get_class();
      std::function<void()> &on_destroy();
    };

    /** Global object base class */
    class global_base_t
    {
    private:
      void fini();
      bool has_interface(const wl_interface *interface) const;

      wl_global *global = nullptr;

    protected:
      struct data_t
      {
        wayland::detail::any user_data;
        std::atomic<unsigned int> counter{1};
      } *data = nullptr;

      global_base_t(display_t &display, const wl_interface* interface, int version, data_t *dat, wl_global_bind_func_t func);

    public:
      global_base_t(wl_global *g);
      global_base_t(const global_base_t& g);
      global_base_t(global_base_t&& g) noexcept;
      ~global_base_t();
      global_base_t &operator=(const global_base_t& g);
      global_base_t &operator=(global_base_t&& g) noexcept;
      bool operator==(const global_base_t& g) const;
      wl_global *c_ptr() const;
      wayland::detail::any &user_data();

      /** Check for specific interface.
       *
       * \tparam resource Resource class for comparison
       * \return true if the global has the same interface as the resource class
       */
      template <typename resource>
      bool has_interface() // instead if wl_global_get_interface
      {
        return has_interface(resource::interface);
      }
    };

    /** Global object.
     *
     * \tparam resource Resource class whose interface shall be used
     */
    template <class resource>
    class global_t : public global_base_t
    {
    private:
      struct data_t : public global_base_t::data_t
      {
        std::function<void(client_t, resource)> bind;
      };

      static void bind_func(wl_client *cl, void *d, uint32_t ver, uint32_t id)
      {
        auto *data = reinterpret_cast<data_t*>(d);
        client_t client(cl);
        resource res(client, ver, id);
        if(data->bind)
          data->bind(client, res);
      }

    public:
      global_t() = delete;

      /** Create a global object
       *
       * \param display Parent display object
       * \param version Interface version
       */
      global_t(display_t &display, unsigned int version = resource::max_version)
        : global_base_t(display, resource::interface, version, new data_t, bind_func)
      {
      }

      /** Adds a listener for the bind signal.
       *
       *  When a client binds to a global object, registered listeners
       *  will be notified, carrying the client_t object and the new
       *  resource_t object.
       */
      std::function<void(client_t, resource)> &on_bind()
      {
        return static_cast<data_t*>(data)->bind;
      }
    };

    struct fd_event_mask_t : public wayland::detail::bitfield<2, -1>
    {
      fd_event_mask_t(const wayland::detail::bitfield<2, -1> &b)
        : wayland::detail::bitfield<2, -1>(b) {}
      fd_event_mask_t(const uint32_t value)
        : wayland::detail::bitfield<2, -1>(value) {}
      static const wayland::detail::bitfield<2, -1> readable;
      static const wayland::detail::bitfield<2, -1> writable;
      static const wayland::detail::bitfield<2, -1> hangup;
      static const wayland::detail::bitfield<2, -1> error;
    };

    class event_loop_t
    {
    private:
      struct data_t
      {
        std::function<void()> destroy;
        detail::listener_t destroy_listener;
        std::list<std::function<int(int, uint32_t)>> fd_funcs;
        std::list<std::function<int()>> timer_funcs;
        std::list<std::function<int(int)>> signal_funcs;
        std::list<std::function<void()>> idle_funcs;
        wayland::detail::any user_data;
        bool do_delete = true;
        std::atomic<unsigned int> counter{1};
      };

      wl_event_loop *event_loop = nullptr;
      data_t *data = nullptr;

      static data_t *wl_event_loop_get_user_data(wl_event_loop *client);
      static void destroy_func(wl_listener *listener, void *data);
      static int event_loop_fd_func(int fd, uint32_t mask, void *data);
      static int event_loop_timer_func(void *data);
      static int event_loop_signal_func(int signal_number, void *data);
      static void event_loop_idle_func(void *data);

    protected:
      event_loop_t(wl_event_loop *p);
      void init();
      void fini();

      friend class display_t;

    public:
      event_loop_t();
      ~event_loop_t();
      event_loop_t(const event_loop_t& e);
      event_loop_t(event_loop_t&& e) noexcept;
      event_loop_t &operator=(const event_loop_t& e);
      event_loop_t &operator=(event_loop_t&& e) noexcept;
      bool operator==(const event_loop_t& e) const;
      wl_event_loop *c_ptr() const;
      wayland::detail::any &user_data();

      /** Create a file descriptor event source
       *
       * \param fd The file descriptor to watch.
       * \param mask A bitwise-or of which events to watch for.
       * \param func The file descriptor dispatch function.
       * \return A new file descriptor event source.
       *
       * The given file descriptor is initially watched for the events given in
       * \c mask. This can be changed as needed with event_source_t::fd_update().
       *
       * If it is possible that program execution causes the file descriptor to be
       * read while leaving the data in a buffer without actually processing it,
       * it may be necessary to register the file descriptor source to be re-checked,
       * see event_source_t::check(). This will ensure that the dispatch function
       * gets called even if the file descriptor is not readable or writable
       * anymore. This is especially useful with IPC libraries that automatically
       * buffer incoming data, possibly as a side-effect of other operations.
       */
      event_source_t add_fd(int fd, const fd_event_mask_t& mask, const std::function<int(int, uint32_t)> &func);

      /** Create a timer event source
       *
       * \param func The timer dispatch function.
       * \return A new timer event source.
       *
       * The timer is initially disarmed. It needs to be armed with a call to
       * event_source_t::timer_update() before it can trigger a dispatch call.
       */
      event_source_t add_timer(const std::function<int()> &func);

      /** Create a POSIX signal event source
       *
       * \param signal_number Number of the signal to watch for.
       * \param func The signal dispatch function.
       * \return A new signal event source.
       *
       * This function blocks the normal delivery of the given signal in the calling
       * thread, and creates a "watch" for it. Signal delivery no longer happens
       * asynchronously, but by wl_event_loop_dispatch() calling the dispatch
       * callback function \c func.
       *
       * It is the caller's responsibility to ensure that all other threads have
       * also blocked the signal.
       */
      event_source_t add_signal(int signal_number, const std::function<int(int)> &func);

      /** Create an idle task
       *
       * \param func The idle task dispatch function.
       * \return A new idle task (an event source).
       *
       * Idle tasks are dispatched before wl_event_loop_dispatch() goes to sleep.
       * See wl_event_loop_dispatch() for more details.
       *
       * Idle tasks fire once, and are automatically destroyed right after the
       * callback function has been called.
       *
       * An idle task can be cancelled before the callback has been called by
       * event_source_t::remove(). Calling event_source_t::remove() after or from
       * within the callback results in undefined behaviour.
       */
      event_source_t add_idle(const std::function<void()> &func);
      const std::function<void()> &on_destroy();

      /** Wait for events and dispatch them
       *
       * \param timeout The polling timeout in milliseconds.
       * \return 0 for success, -1 for polling error.
       *
       * All the associated event sources are polled. This function blocks until
       * any event source delivers an event (idle sources excluded), or the timeout
       * expires. A timeout of -1 disables the timeout, causing the function to block
       * indefinitely. A timeout of zero causes the poll to always return immediately.
       *
       * All idle sources are dispatched before blocking. An idle source is destroyed
       * when it is dispatched. After blocking, all other ready sources are
       * dispatched. Then, idle sources are dispatched again, in case the dispatched
       * events created idle sources. Finally, all sources marked with
       * event_source_t::check() are dispatched in a loop until their dispatch
       * functions all return zero.
       */
      int dispatch(int timeout) const;

      /** Dispatch the idle sources
       */
      void dispatch_idle() const;

      /** Get the event loop file descriptor
       *
       * \return The aggregate file descriptor.
       *
       * This function returns the aggregate file descriptor, that represents all
       * the event sources (idle sources excluded) associated with the given event
       * loop context. When any event source makes an event available, it will be
       * reflected in the aggregate file descriptor.
       *
       * When the aggregate file descriptor delivers an event, one can call
       * event_loop_t::dispatch() on the event loop context to dispatch all the
       * available events.
       */
      int get_fd() const;
    };

    class event_source_t : public wayland::detail::refcounted_wrapper<wl_event_source>
    {
    private:
      wl_event_source *event_source = nullptr;

    protected:
      event_source_t(wl_event_source *p);
      friend class event_loop_t;

    public:
      event_source_t() = delete;
      wl_event_source *c_ptr() const;

      /** Arm or disarm a timer
       *
       * \param ms_delay The timeout in milliseconds.
       * \return 0 on success, -1 on failure.
       *
       * If the timeout is zero, the timer is disarmed.
       *
       * If the timeout is non-zero, the timer is set to expire after the given
       * timeout in milliseconds. When the timer expires, the dispatch function
       * set with event_loop_t::add_timer() is called once from
       * event_loop_t::dispatch(). If another dispatch is desired after another
       * expiry, event_source_t::timer_update() needs to be called again.
       */
      int timer_update(int ms_delay) const;

      /** Update a file descriptor source's event mask
       *
       * \param mask The new mask.
       * \return 0 on success, -1 on failure.
       *
       * This changes which events, readable and/or writable, cause the dispatch
       * callback to be called on.
       *
       * File descriptors are usually writable to begin with, so they do not need to
       * be polled for writable until a write actually fails. When a write fails,
       * the event mask can be changed to poll for readable and writable, delivering
       * a dispatch callback when it is possible to write more. Once all data has
       * been written, the mask can be changed to poll only for readable to avoid
       * busy-looping on dispatch.
       */
      int fd_update(const fd_event_mask_t& mask) const;

      /** Mark event source to be re-checked
       *
       * This function permanently marks the event source to be re-checked after
       * the normal dispatch of sources in event_loop_t::dispatch(). Re-checking
       * will keep iterating over all such event sources until the dispatch
       * function for them all returns zero.
       *
       * Re-checking is used on sources that may become ready to dispatch as a
       * side-effect of dispatching themselves or other event sources, including idle
       * sources. Re-checking ensures all the incoming events have been fully drained
       * before event_loop_t::dispatch() returns.
       */
      void check() const;
    };
  }
}

#endif
