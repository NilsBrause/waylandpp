/*
 * Copyright (c) 2014-2022, Nils Christopher Brause, Philipp Kerling
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

/** \file */

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <wayland-version.hpp>
#include <wayland-client-core.h>
#include <wayland-util.hpp>

namespace wayland
{
  /** \brief Type for functions that handle log messages
   *
   * Log message is the first argument
   */
  using log_handler = std::function<void(std::string)> ;

  /** \brief Set C library log handler
   *
   * The C library sometimes logs important information such as protocol
   * error messages, by default to the standard output. This can be used
   * to set an alternate function that will receive those messages.
   *
   * \param handler function that should be called for C library log messages
   */
  void set_log_handler(log_handler handler);

  /** \brief A queue for proxy_t object events.

      Event queues allows the events on a display to be handled in a
      thread-safe manner. See display_t for details.
  */
  class event_queue_t : public detail::refcounted_wrapper<wl_event_queue>
  {
    event_queue_t(wl_event_queue *q);
    friend class display_t;
  public:
    event_queue_t() = default;
    event_queue_t(const event_queue_t&) = default;
    event_queue_t(event_queue_t&&) noexcept = default;
    event_queue_t& operator=(const event_queue_t&) = default;
    event_queue_t& operator=(event_queue_t&&) noexcept = default;
    ~event_queue_t() noexcept = default;
  };

  class display_t;

  namespace detail
  {
    struct proxy_data_t;
    // base class for event listener storage.
    struct events_base_t
    {
      events_base_t() = default;
      events_base_t(const events_base_t&) = default;
      events_base_t(events_base_t&&) noexcept = default;
      events_base_t& operator=(const events_base_t&) = default;
      events_base_t& operator=(events_base_t&&) noexcept = default;
      virtual ~events_base_t() noexcept = default;
    };
  }

  /** \brief Represents a protocol object on the client side.

      A proxy_t acts as a client side proxy to an object existing in the
      compositor. The proxy is responsible for converting requests made
      by the clients with proxy_t::marshal() or
      proxy_t::marshal_constructor() into Wayland's wire format. Events
      coming from the compositor are also handled by the proxy, which
      will in turn call the handler set with the on_XXX() functions of
      each interface class.

      With the exception of the function proxy_t::set_queue(), functions
      accessing a proxy_t are not normally used by client code. Clients
      should normally use the higher level interface classed generated
      by the scanner to interact with compositor objects.
  */
  class proxy_t
  {
  public:
    /**
     * Underlying wl_proxy type and properties of a proxy_t that affect construction,
     * destruction, and event handling
     */
    enum class wrapper_type
    {
      /**
       * C pointer is a standard type compatible with wl_proxy*. Events are dispatched
       * and it is destructed when the proxy_t is destructed. User data is set.
       */
      standard,
      /**
       * C pointer is a wl_display*. No events are dispatched, wl_display_disconnect
       * is called when the proxy_t is destructed. User data is set.
       */
      display,
      /**
       * C pointer is a standard type compatible with wl_proxy*, but another library
       * owns it and it should not be touched in a way that could affect the
       * operation of the other library. No events are dispatched, wl_proxy_destroy
       * is not called when the proxy_t is destructed, user data is not touched.
       * Consequently, there is no reference counting for the proxy_t.
       * Lifetime of such wrappers should preferably be short to minimize the chance
       * that the owning library decides to destroy the wl_proxy.
       */
      foreign,
      /**
       * C pointer is a wl_proxy* that was constructed with wl_proxy_create_wrapper.
       * No events are dispatched, wl_proxy_wrapper_destroy is called when the
       * proxy_t is destroyed. Reference counting is active. A reference to the
       * proxy_t creating this proxy wrapper is held to extend its lifetime until
       * after the proxy wrapper is destroyed.
       */
      proxy_wrapper
    };

  private:
    wl_proxy *proxy = nullptr;
    detail::proxy_data_t *data = nullptr;
    wrapper_type type = wrapper_type::standard;
    friend class detail::argument_t;
    friend struct detail::proxy_data_t;

    // Interface desctiption filled in by the each interface class
    const wl_interface *interface = nullptr;

    // copy constructor filled in by the each interface class
    std::function<proxy_t(proxy_t)> copy_constructor;

    // universal dispatcher
    static int c_dispatcher(const void *implementation, void *target,
                            uint32_t opcode, const wl_message *message,
                            wl_argument *args);

    // marshal request
    proxy_t marshal_single(uint32_t opcode, const wl_interface *interface,
                           const std::vector<detail::argument_t>& args, std::uint32_t version = 0);

  protected:
    void set_interface(const wl_interface *iface);
    void set_copy_constructor(const std::function<proxy_t(proxy_t)>& func);

    friend class registry_t;
    // marshal a request, that doesn't lead a new proxy
    // Valid types for args are:
    // - uint32_t
    // - int32_t
    // - proxy_t
    // - std::string
    // - array_t
    template <typename...T>
    void marshal(uint32_t opcode, const T& ...args)
    {
      std::vector<detail::argument_t> v = { detail::argument_t(args)... };
      marshal_single(opcode, nullptr, v);
    }

    // marshal a request that leads to a new proxy with inherited version
    template <typename...T>
    proxy_t marshal_constructor(uint32_t opcode, const wl_interface *interface,
                                const T& ...args)
    {
      std::vector<detail::argument_t> v = { detail::argument_t(args)... };
      return marshal_single(opcode, interface, v);
    }

    // marshal a request that leads to a new proxy with specific version
    template <typename...T>
    proxy_t marshal_constructor_versioned(uint32_t opcode, const wl_interface *interface,
                                          uint32_t version, const T& ...args)
    {
      std::vector<detail::argument_t> v = { detail::argument_t(args)... };
      return marshal_single(opcode, interface, v, version);
    }

    // Set the opcode for destruction of the proxy
    void set_destroy_opcode(uint32_t destroy_opcode);

    /*
      Sets the dispatcher and its user data. User data must be an
      instance of a class derived from events_base_t, allocated with
      new. Will automatically be deleted upon destruction.
    */
    void set_events(std::shared_ptr<detail::events_base_t> events,
                    int(*dispatcher)(uint32_t, const std::vector<detail::any>&, const std::shared_ptr<detail::events_base_t>&));

    // Retrieve the previously set user data
    std::shared_ptr<detail::events_base_t> get_events();

    // Constructs NULL proxies.
    proxy_t() = default;

    struct construct_proxy_wrapper_tag {};
    // Construct from proxy as wrapper
    proxy_t(const proxy_t &wrapped_proxy, construct_proxy_wrapper_tag /*unused*/);

  public:
    /** \brief Cronstruct a proxy_t from a wl_proxy pointer
        \param p Pointer to a wl_proxy
        \param t type and requested behavior of the pointer
        \param queue initial event queue reference to retain (set_queue is not called)
    */
    proxy_t(wl_proxy *p, wrapper_type t = wrapper_type::standard, event_queue_t const& queue = event_queue_t());

    /** \brief Copy Constructior
        \param p A proxy_t object

        Creates a copy of the proxy_t object that points to the sme proxy.
    */
    proxy_t(const proxy_t &p);

    /** \brief Assignment operator
        \param p A proxy_t object

        After an assignment, both proxy_t objects will point to the same proxy.
    */
    proxy_t &operator=(const proxy_t &p);

    /** \brief Move Constructior
        \param p A proxy_t object

        Transfers the contents of the proxy_t object on the right
        to the proxy_t object being constructed.
    */
    proxy_t(proxy_t &&p) noexcept;

    /** \brief Move Asignment operator
        \param p A proxy_t object

        Swaps the contents of both proxy_t objects.
    */
    proxy_t &operator=(proxy_t &&p) noexcept;

    /** \brief Destructor

        If this is the last copy of a paticular proxy, the proxy itself will
        be detroyed and an destroy request will be marshaled.
        If the proxy belongs to a wl_display object, the connection will be
        closed.
        Special rules apply to proxy wrappers and foreign proxies.
        See \ref wrapper_type for more infos.
    */
    ~proxy_t();

    /** \brief Get the id of a proxy object.
        \return The id the object associated with the proxy
    */
    uint32_t get_id() const;

    /** \brief Get the interface name (class) of a proxy object.
        \return The interface name of the object associated with the proxy
    */
    std::string get_class() const;

    /** \brief Get the protocol object version of a proxy object.
     *
     * Gets the protocol object version of a proxy object, or 0 if the proxy was
     * created with unversioned API.
     *
     * A returned value of 0 means that no version information is available,
     * so the caller must make safe assumptions about the object's real version.
     *
     * \ref display_t will always return version 0.
     *
     * \return The protocol object version of the proxy or 0
     */
    uint32_t get_version() const;

    /** \brief Get the type of a proxy object.
     */
    wrapper_type get_wrapper_type() const
    {
      return type;
    }

    /** \brief Assign a proxy to an event queue.
        \param queue The event queue that will handle this proxy

        Assign proxy to event queue. Events coming from proxy will be queued in
        queue instead of the display's main queue.

        See also: display_t::dispatch_queue().
    */
    void set_queue(event_queue_t queue);

    /** \brief Get a pointer to the underlying C struct.
     *  \return The underlying wl_proxy wrapped by this proxy_t if it exists,
     *          otherwise an exception is thrown
     */
    wl_proxy *c_ptr() const;

    /** \brief Check whether this wrapper actually wraps an object
     *  \return true if there is an underlying object, false if this wrapper is
     *          empty
     */
    bool proxy_has_object() const;

    /** \brief Check whether this wrapper actually wraps an object
     *  \return true if there is an underlying object, false if this wrapper is
     *          empty
     */
    operator bool() const;

    /** \brief Check whether two wrappers refer to the same object
     */
    bool operator==(const proxy_t &right) const;

    /** \brief Check whether two wrappers refer to different objects
     */
    bool operator!=(const proxy_t &right) const;

    /** \brief Release the wrapped object (if any), making this an empty wrapper
     *
     * Note that display_t instances cannot be released this way. Attempts to
     * do so are ignored.
     */
    void proxy_release();
  };

  /** \brief Represents an intention to read from the display file
   *  descriptor
   *
   * When not using the convenience method \ref display_t::dispatch that takes care
   * of this automatically, threads that want to read events from a Wayland
   * display file descriptor must announce their intention to do so beforehand - in the C API,
   * this is done using wl_display_prepare_read. This intention must then be
   * resolved either by actually invoking a read from the file descriptor or cancelling.
   *
   * This RAII class makes sure that when it goes out of scope, the intent
   * is cancelled automatically if it was not finalized by manually cancelling
   * or reading before. Otherwise, it would be easy to forget resolving the
   * intent e.g. when handling errors, potentially leading to a deadlock.
   *
   * Read intents can only be created by a \ref display_t with
   * \ref display_t::obtain_read_intent and \ref display_t::obtain_queue_read_intent.
   *
   * Undefined behavior occurs when the associated \ref display_t or
   * \ref event_queue_t is destroyed when a read_intent has not been finalized yet.
   */
  class read_intent
  {
  public:
    read_intent(read_intent &&other) noexcept = default;
    read_intent(read_intent const &other) = delete;
    read_intent& operator=(read_intent const &other) = delete;
    read_intent& operator=(read_intent &&other) noexcept = delete;
    ~read_intent();

    /** \brief Check whether this intent was already finalized with \ref cancel
     * or \ref read
     */
    bool is_finalized() const;

    /** \brief Cancel read intent
     *
     * An exception is thrown when the read intent was already finalized.
     */
    void cancel();

    /** \brief Read events from display file descriptor
     *
     * This will read events from the file descriptor for the
     * display. This function does not dispatch events, it only reads
     * and queues events into their corresponding event queues. If no
     * data is avilable on the file descriptor, read() returns immediately.
     * To dispatch events that may have been queued, call
     * \ref display_t::dispatch_pending or \ref display_t::dispatch_queue_pending.
     *
     * An exception is thrown when the read intent was already finalized.
     * Each read intent can only be used for reading once. A new one must be
     * obtained for any further read requests.
     */
    void read();

  private:
    read_intent(wl_display *display, wl_event_queue *event_queue = nullptr);
    friend class display_t;

    wl_display *display;
    wl_event_queue *event_queue = nullptr;
    bool finalized = false;
  };

  class callback_t;
  class registry_t;

  /** \brief Represents a connection to the compositor and acts as a
      proxy to the display singleton object.

      A display_t object represents a client connection to a Wayland
      compositor. It is created with display_t::display_t(). A
      connection is terminated using display_t::~display_t().

      A display_t is also used as the proxy for the display singleton
      object on the compositor side. A display_t object handles all the
      data sent from and to the compositor. When a proxy_t marshals a
      request, it will write its wire representation to the display's
      write buffer. The data is sent to the compositor when the client
      calls display_t::flush().

      Incoming data is handled in two steps: queueing and
      dispatching. In the queue step, the data coming from the display
      fd is interpreted and added to a queue. On the dispatch step, the
      handler for the incoming event set by the client on the
      corresponding proxy_t is called.

      A display has at least one event queue, called the main
      queue. Clients can create additional event queues with
      display_t::create_queue() and assign proxy_t's to it. Events
      occurring in a particular proxy are always queued in its assigned
      queue. A client can ensure that a certain assumption, such as
      holding a lock or running from a given thread, is true when a
      proxy event handler is called by assigning that proxy to an event
      queue and making sure that this queue is only dispatched when the
      assumption holds.

      The main queue is dispatched by calling display_t::dispatch().
      This will dispatch any events queued on the main queue and attempt
      to read from the display fd if its empty. Events read are then
      queued on the appropriate queues according to the proxy
      assignment. Calling that function makes the calling thread the
      main thread.

      A user created queue is dispatched with
      display_t::dispatch_queue(). If there are no events to dispatch
      this function will block. If this is called by the main thread,
      this will attempt to read data from the display fd and queue any
      events on the appropriate queues. If calling from any other
      thread, the function will block until the main thread queues an
      event on the queue being dispatched.

      A real world example of event queue usage is Mesa's implementation
      of eglSwapBuffers() for the Wayland platform. This function might
      need to block until a frame callback is received, but dispatching
      the main queue could cause an event handler on the client to start
      drawing again. This problem is solved using another event queue,
      so that only the events handled by the EGL code are dispatched
      during the block.

      This creates a problem where the main thread dispatches a non-main
      queue, reading all the data from the display fd. If the
      application would call poll(2) after that it would block, even
      though there might be events queued on the main queue. Those
      events should be dispatched with display_t::dispatch_pending()
      before flushing and blocking.
  */
  class display_t : public proxy_t
  {
  private:
    // Construct as proxy wrapper
    display_t(proxy_t const &wrapped_proxy, construct_proxy_wrapper_tag /*unused*/);

  public:
    /** \brief Connect to Wayland display on an already open fd.
        \param fd The fd to use for the connection

        The display_t takes ownership of the fd and will close it when
        the display is destroyed. The fd will also be closed in case of
        failure.
    */
    display_t(int fd);

    display_t(display_t &&d) noexcept;
    display_t(const display_t &d) = delete;
    display_t &operator=(const display_t &d) = delete;
    display_t &operator=(display_t &&d) noexcept;

    /**  \brief Connect to a Wayland display.
         \param name Optional name of the Wayland display to connect to

         Connect to the Wayland display named name. If name is empty,
         its value will be replaced with the WAYLAND_DISPLAY environment
         variable if it is set, otherwise display "wayland-0" will be
         used.
    */
    display_t(const std::string& name = {});

    /** \brief Use an existing connection to a Wayland display to
        construct a waylandpp display_t
        \param display C wl_display pointer to use; must not be nullptr

        A wl_display* that was already established using the C wayland-client
        API is wrapped in an waylandpp display_t instance so it can be used
        easily from C++. Ownership of the display is not taken, so this may
        be used for wrapping a wl_display connection established by another library.

        On destruction of the display_t, wl_display_disconnect is not called and
        no resources are freed.
        It is the responsibility of the caller to make sure that the wl_display
        and the display_t are not used simultaneously in incompatible ways. It is
        especially problematic if the wl_display is destroyed while the display_t
        wrapper is still being used.

        Whether the wl_display or the display_t is destructed first ultimately
        does not matter, but any waylandpp proxy_t instances must be destructed
        or have their owned objects released before the wl_display is destroyed.
        Otherwise, the proxy_t destructor will try to free the underlying wl_proxy
        that was already destroyed together with the wl_display.
    */
    explicit display_t(wl_display* display);

    /** \brief Close a connection to a Wayland display.

        Close the connection to display and free all resources
        associated with it.
        This does not apply to display_t instances that are wrappers for
        a pre-established C wl_display.
    */
    ~display_t() noexcept = default;

    /** \brief Create a new event queue for this display.
        \return A new event queue associated with this display or NULL
        on failure.
    */
    event_queue_t create_queue() const;

    /** \brief Get a display context's file descriptor.
        \return Display object file descriptor

        Return the file descriptor associated with a display so it can
        be integrated into the client's main loop.
    */
    int get_fd() const;

    /** \brief Block until all pending request are processed by the server.
        \return The number of dispatched events
        \exception std::system_error on failure

        Blocks until the server process all currently issued requests
        and sends out pending events on all event queues.
    */
    int roundtrip() const;

    /** \brief Block until all pending request are processed by the server.
        \return The number of dispatched events
        \exception std::system_error on failure

        Blocks until the server processes all currently issued requests
        and sends out pending events on the event queue.

        Note: This function uses dispatch_queue() internally. If you are using
        read_events() from more threads, don't use this function (or make sure
        that calling roundtrip_queue() doesn't interfere with calling
        prepare_read() and read_events())
    */
    int roundtrip_queue(const event_queue_t& queue) const;

    /** \brief Announce calling thread's intention to read events from the
     * Wayland display file descriptor
     *
     * This ensures that until the thread is ready to read and calls \ref
     * read_intent::read, no other thread will read from the file descriptor.
     * During preparation, all undispatched events in the event queue
     * are dispatched until the queue is empty.
     *
     * Use this function before polling on the display fd or to integrate the
     * fd into a toolkit event loop in a race-free way.
     *
     * Typical usage is:
     *
     * \code
     * auto read_intent = display.obtain_read_intent();
     * display.flush();
     * poll(fds, nfds, -1); // Custom poll() handling is possible here
     * if(fd.revents & POLLIN)
     *   read_intent.read();
     * display.dispatch_pending();
     * \endcode
     *
     * The \ref read_intent ensures that if the above code e.g. throws an
     * exception before actually reading from the file descriptor or times out
     * in poll(), the read intent is always cancelled so other threads can proceed.
     *
     * In one thread, do not hold more than one read intent for the same
     * display at the same time, irrespective of the event queue.
     *
     * \return New \ref read_intent for this display and the default event queue
     * \exception std::system_error on failure
     */
    read_intent obtain_read_intent() const;

    /** \brief Announce calling thread's intention to read events from the
     * Wayland display file descriptor
     *
     * \param queue event queue for which the read event will be valid
     * \return New \ref read_intent for this display and the specified event queue
     * \exception std::system_error on failure
     *
     * See \ref obtain_read_intent for details.
     */
    read_intent obtain_queue_read_intent(const event_queue_t& queue) const;

    /** \brief Dispatch events in an event queue.
        \param queue The event queue to dispatch
        \return The number of dispatched events
        \exception std::system_error on failure

        Dispatch all incoming events for objects assigned to the given
        event queue. On failure -1 is returned and errno set
        appropriately.

        This function blocks if there are no events to dispatch. If
        calling from the main thread, it will block reading data from
        the display fd. For other threads this will block until the main
        thread queues events on the queue passed as argument.
    */
    int dispatch_queue(const event_queue_t& queue) const;

    /** \brief Dispatch pending events in an event queue.
        \param queue The event queue to dispatch
        \return The number of dispatched events
        \exception std::system_error on failure

        Dispatch all incoming events for objects assigned to the given
        event queue. On failure -1 is returned and errno set
        appropriately. If there are no events queued, this function
        returns immediately.
    */
    int dispatch_queue_pending(const event_queue_t& queue) const;

    /** \brief Process incoming events.
        \return The number of dispatched events
        \exception std::system_error on failure

        Dispatch the display's main event queue.

        If the main event queue is empty, this function blocks until
        there are events to be read from the display fd. Events are read
        and queued on the appropriate event queues. Finally, events on
        the main event queue are dispatched.

        Note: It is not possible to check if there are events on the
        main queue or not. For dispatching main queue events without
        blocking, see display_t::dispatch_pending(). Calling this will
        release the display file descriptor if this thread acquired it
        using display_t::acquire_fd().

        See also: display_t::dispatch_pending(),
        display_t::dispatch_queue()
    */
    int dispatch() const;

    /** \brief Dispatch main queue events without reading from the display fd.
        \return The number of dispatched events
        \exception std::system_error on failure

        This function dispatches events on the main event queue. It
        does not attempt to read the display fd and simply returns zero
        if the main queue is empty, i.e., it doesn't block.

        This is necessary when a client's main loop wakes up on some fd
        other than the display fd (network socket, timer fd, etc) and
        calls wl_display_dispatch_queue() from that callback. This may
        queue up events in the main queue while reading all data from
        the display fd. When the main thread returns to the main loop
        to block, the display fd no longer has data, causing a call to
        poll(2) (or similar functions) to block indefinitely, even
        though there are events ready to dispatch.

        To proper integrate the wayland display fd into a main loop,
        the client should always call display_t::dispatch_pending() and
        then display_t::flush() prior to going back to sleep. At that
        point, the fd typically doesn't have data so attempting I/O
        could block, but events queued up on the main queue should be
        dispatched.

        A real-world example is a main loop that wakes up on a timerfd
        (or a sound card fd becoming writable, for example in a video
        player), which then triggers GL rendering and eventually
        eglSwapBuffers(). eglSwapBuffers() may call
        display_t::dispatch_queue() if it didn't receive the frame
        event for the previous frame, and as such queue events in the
        main queue. Note: Calling this makes the current thread the
        main one.

        See also: display_t::dispatch(), display_t::dispatch_queue(),
        display_t::flush()
    */
    int dispatch_pending() const;

    /** \brief Retrieve the last error that occurred on a display.
        \return The last error that occurred on display or 0 if no error
        occurred

        Return the last error that occurred on the display. This may be
        an error sent by the server or caused by the local client.

        Note: Errors are fatal. If this function returns non-zero the
        display can no longer be used.
    */
    int get_error() const;

    /** \brief Send all buffered requests on the display to the server.
        \return Tuple of the number of bytes sent and whether all data
        was sent.
        \exception std::system_error on failure

        Send all buffered data on the client side to the server. Clients
        should call this function before blocking. On success, the
        number of bytes sent to the server is returned.

        display_t::flush() never blocks. It will write as much data as
        possible, but if all data could not be written, the second element
        in the returned tuple will be set to false. In that case, use poll on the
        display file descriptor to wait for it to become writable again.
    */
    std::tuple<int, bool> flush() const;

    /** \brief asynchronous roundtrip

        The sync request asks the server to emit the 'done' event on
        the returned callback_t object. Since requests are handled
        in-order and events are delivered in-order, this can be used as
        a barrier to ensure all previous requests and the resulting
        events have been handled.

        The object returned by this request will be destroyed by the
        compositor after the callback is fired and as such the client
        must not attempt to use it after that point.
    */
    callback_t sync();

    /** \brief get global registry object

        This request creates a registry object that allows the client to
        list and bind the global objects available from the compositor.
    */
    registry_t get_registry();

    operator wl_display*() const;

    /** \brief create proxy wrapper for this display
     */
    display_t proxy_create_wrapper();
  };
}

#include <wayland-client-protocol.hpp>

#endif
