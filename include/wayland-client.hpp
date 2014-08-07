/*
 * Copyright (c) 2014, Nils Christopher Brause
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

#include <memory>
#include <string>
#include <vector>
#include <wayland-client.h>
#include <any.hpp>

/** \brief A queue for proxy_t object events. 
    
    Event queues allows the events on a display to be handled in a
    thread-safe manner. See display_t for details.
 */
class event_queue_t
{
private:
  struct queue_ptr
  {
    wl_event_queue *queue;
    ~queue_ptr();
  };

  std::shared_ptr<queue_ptr> queue;
  event_queue_t(wl_event_queue *q);
  friend class display_t;

public:
  /** \brief Get a pointer to the underlying C struct.
      \return Pointer to a wl_event_queue.
   */
  wl_event_queue *c_ptr();
};


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
protected:
  // base class for event listener storage.
  struct events_base_t
  {
    virtual ~events_base_t() { }
  };

private:
  // stored in the proxy user data
  struct proxy_data_t
  {
    proxy_t *proxy;
    std::shared_ptr<events_base_t> events;
    int opcode; 
    // 
    unsigned int counter;
  };

  wl_proxy *proxy;
  bool display;

  // universal dispatcher
  static int c_dispatcher(const void *implementation, void *target,
                          uint32_t opcode, const wl_message *message,
                          wl_argument *args);

  // marshal request
  proxy_t marshal_single(uint32_t opcode, const wl_interface *interface,
                         std::vector<wl_argument> v);

  // handles integers, file descriptors and fixed point numbers
  // (this works, because wl_argument is an union)
  wl_argument conv(uint32_t i);
  // handles strings
  wl_argument conv(std::string s);
  // handles objects
  wl_argument conv(proxy_t p);
  // handles arrays
  wl_argument conv(std::vector<char> a);

protected:
  // Interface desctiption filled in by the each interface class
  const wl_interface *interface;

  friend class registry_t;

  // Event dispatcher. Overwritten by each interface class.
  virtual int dispatcher(int opcode, std::vector<any> args);

  // marshal a request, that doesn't lead a new proxy
  // Valid types for args are:
  // - uint32_t
  // - int32_t
  // - proxy_t
  // - std::string
  // - std::vector<char>
  template <typename...T>
  void marshal(uint32_t opcode, T...args)
  {
    std::vector<wl_argument> v = { conv(args)... };
    if(c_ptr())
      marshal_single(opcode, NULL, v);
  }

  // marshal a request, that leads a new proxy
  template <typename...T>
  proxy_t marshal_constructor(uint32_t opcode, const wl_interface *interface,
                              T...args)
  {
    std::vector<wl_argument> v = { conv(args)... };
    if(c_ptr())
      return marshal_single(opcode, interface, v);
    return proxy_t();
  }

  // Set the opcode for destruction of the proxy (-1 unsets it)
  void set_destroy_opcode(int destroy_opcode);

  /*
    Set user data for the dispatcher. Must be an instance of a class
    derived from events_base_t, allocated wthi new. Will automatically
    be deleted upon destruction.
  */
  void set_events(std::shared_ptr<events_base_t> events);

  // Retrieve the perviously set user data
  std::shared_ptr<events_base_t> get_events();

  // Constructs NULL proxies.
  proxy_t();

public:
  /** \brief Cronstruct a proxy_t from a wl_proxy pointer
      \param p Pointer to a wl_proxy
      \param is_display True, if p is a wl_display pointer
  */
  proxy_t(wl_proxy *p, bool is_display = false);

  /** \brief Copy Constructior
      \param p A proxy_t object

      For details see operator=()
  */
  proxy_t(const proxy_t& p);

  /** \brief Assignment operator
      \param p A proxy_t object

      After an assignment, both proxy_t objects will point to the same proxy.
  */
  proxy_t &operator=(const proxy_t &p);

  /** \brief Destructor

      If this is the last copy of a paticular proxy, the proxy itself will
      be detroyed and an destroy request will be marshaled.
      If the proxy belongs to a wl_display object, the connection will be
      closed.
  */
  ~proxy_t();

  /** \brief Get the id of a proxy object.
      \return The id the object associated with the proxy
   */
  uint32_t get_id();

  /** \brief Get the interface name (class) of a proxy object. 
      \return The interface name of the object associated with the proxy 
   */
  std::string get_class();

  /** \brief Assign a proxy to an event queue.
      \param queue The event queue that will handle this proxy 

      Assign proxy to event queue. Events coming from proxy will be queued in
      queue instead of the display's main queue.

      See also: display_t::dispatch_queue().
  */
  void set_queue(event_queue_t queue);

  /** \brief Get a pointer to the underlying C struct.
      \return Pointer to a wl_proxy.
   */
  wl_proxy *c_ptr();
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
  display_t(display_t &d) { }

public:
  /** \brief Connect to Wayland display on an already open fd.
      \param fd The fd to use for the connection
      
      The display_t takes ownership of the fd and will close it when
      the display is destroyed. The fd will also be closed in case of
      failure.
   */
  display_t(int fd);

  /**  \brief Connect to a Wayland display. 
       \param name Optional name of the Wayland display to connect to

       Connect to the Wayland display named name. If name is empty,
       its value will be replaced with the WAYLAND_DISPLAY environment
       variable if it is set, otherwise display "wayland-0" will be
       used.
   */
  display_t(std::string name = "");

  /** \brief Close a connection to a Wayland display.

      Close the connection to display and free all resources
      associated with it.
   */
  ~display_t();

  /** \brief Create a new event queue for this display.
      \return A new event queue associated with this display or NULL
      on failure.
  */ 
  event_queue_t create_queue();

  /** \brief Get a display context's file descriptor.
      \return Display object file descriptor

      Return the file descriptor associated with a display so it can
      be integrated into the client's main loop.
  */
  int get_fd();

  /** \brief Block until all pending request are processed by the server.
      \return The number of dispatched events on success or -1 on failure

      Blocks until the server process all currently issued requests
      and sends out pending events on all event queues.
  */
  int roundtrip();

  /** \brief Read events from display file descriptor.
      \return 0 on success or -1 on error. In case of error errno will
      be set accordingly
      
      This will read events from the file descriptor for the
      display. This function does not dispatch events, it only reads
      and queues events into their corresponding event queues. If no
      data is avilable on the file descriptor,
      display_t::read_events() returns immediately. To dispatch events
      that may have been queued, call display_t::dispatch_pending() or
      display_t::dispatch_queue_pending(). Before calling this
      function, display_t::prepare_read() must be called first.
   */
  int read_events();

  /** \brief Prepare to read events after polling file descriptor. 
      \return 0 on success or -1 if event queue was not empty 
      
      This function must be called before reading from the file
      descriptor using display_t::read_events(). Calling
      display_t::prepare_read() announces the calling threads
      intention to read and ensures that until the thread is ready to
      read and calls display_t::read_events(), no other thread will
      read from the file descriptor. This only succeeds if the event
      queue is empty though, and if there are undispatched events in
      the queue, -1 is returned and errno set to EAGAIN.

      If a thread successfully calls display_t::prepare_read(), it
      must either call display_t::read_events() when it's ready or
      cancel the read intention by calling display_t::cancel_read().

      Use this function before polling on the display fd or to
      integrate the fd into a toolkit event loop in a race-free
      way. Typically, a toolkit will call
      display_t::dispatch_pending() before sleeping, to make sure it
      doesn't block with unhandled events. Upon waking up, it will
      assume the file descriptor is readable and read events from the
      fd by calling display_t::dispatch(). Simplified, we have:

      
      \code{.cpp}
      display.dispatch_pending();
      display.flush();
      poll(fds, nfds, -1);
      display.dispatch();
      \endcode

      There are two races here: first, before blocking in poll(), the
      fd could become readable and another thread reads the
      events. Some of these events may be for the main queue and the
      other thread will queue them there and then the main thread will
      go to sleep in poll(). This will stall the application, which
      could be waiting for a event to kick of the next animation
      frame, for example.

      The other race is immediately after poll(), where another thread
      could preempt and read events before the main thread calls
      display_t::dispatch(). This call now blocks and starves the
      other fds in the event loop.

      A correct sequence would be:

      \code{.cpp}
      while(display.prepare_read() != 0)
        display.dispatch_pending();
      display.flush();
      poll(fds, nfds, -1);
      display.read_events();
      display.dispatch_pending();
      \endcode
      
      Here we call display_t::prepare_read(), which ensures that
      between returning from that call and eventually calling
      display_t::read_events(), no other thread will read from the fd
      and queue events in our queue. If the call to
      display_t::prepare_read() fails, we dispatch the pending events
      and try again until we're successful.
  */
  int prepare_read();
  
  /** \brief Prepare to read events after polling file descriptor. 
      \param queue The event queue to prepare read from
      \return 0 on success or -1 if event queue was not empty

      See display_t::prepare_read() for details.
  */      

  int prepare_read_queue(event_queue_t queue);

  /** \brief Release exclusive access to display file descriptor.
  
      This releases the exclusive access. Useful for canceling the
      lock when a timed out poll returns fd not readable and we're not
      going to read from the fd anytime soon.
  */
  void cancel_read();

  /** \brief Dispatch events in an event queue. 
      \param queue The event queue to dispatch
      \return The number of dispatched events on success or -1 on failure 

      Dispatch all incoming events for objects assigned to the given
      event queue. On failure -1 is returned and errno set
      appropriately.

      This function blocks if there are no events to dispatch. If
      calling from the main thread, it will block reading data from
      the display fd. For other threads this will block until the main
      thread queues events on the queue passed as argument.
  */ 
  int dispatch_queue(event_queue_t queue);

  /** \brief Dispatch pending events in an event queue.
      \param queue The event queue to dispatch 
      \return The number of dispatched events on success or -1 on failure

      Dispatch all incoming events for objects assigned to the given
      event queue. On failure -1 is returned and errno set
      appropriately. If there are no events queued, this function
      returns immediately.
  */
  int dispatch_queue_pending(event_queue_t queue);
      
  /** \brief Process incoming events. 
      \return The number of dispatched events on success or -1 on failure 

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
  int dispatch();

  /** \brief Dispatch main queue events without reading from the display fd.
      \return The number of dispatched events or -1 on failure
      
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
  int dispatch_pending();

  /** \brief Retrieve the last error that occurred on a display.
      \return The last error that occurred on display or 0 if no error
      occurred

      Return the last error that occurred on the display. This may be
      an error sent by the server or caused by the local client.

      Note: Errors are fatal. If this function returns non-zero the
      display can no longer be used.
  */
  int get_error();

  /** \brief Send all buffered requests on the display to the server. 
      \return The number of bytes sent on success or -1 on failure
      
      Send all buffered data on the client side to the server. Clients
      should call this function before blocking. On success, the
      number of bytes sent to the server is returned. On failure, this
      function returns -1 and errno is set appropriately.

      display_t::flush() never blocks. It will write as much data as
      possible, but if all data could not be written, errno will be
      set to EAGAIN and -1 returned. In that case, use poll on the
      display file descriptor to wait for it to become writable again.
  */
  int flush();

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

  /* \brief get global registry object
     
     This request creates a registry object that allows the client to
     list and bind the global objects available from the compositor.
  */
  registry_t get_registry();
 };

/** \brief These errors are global and can be emitted in response to
    any server request.
 */
enum display_error
  {
    /** \brief server couldn't find object 
     */
    display_error_invalid_object = 0,

    /** \brief method doesn't exist on the specified interface
     */
    display_error_invalid_method = 1,

    /** \brief server is out of memory
     */
    display_error_no_memory = 2
  };

#include <wayland-client-protocol.hpp>

#endif
