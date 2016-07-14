'use strict';

  Polymer({
    is: 'carbon-route-converter',

    properties: {
      /**
       * A model representing the deserialized path through the route tree, as
       * well as the current queryParams.
       *
       * A route object is the kernel of the routing system. It is intended to
       * be fed into consuming elements such as `carbon-route`.
       *
       * @type {?Object}
       */
      route: {
        type: Object,
        value: null,
        notify: true
      },

      /**
       * A set of key/value pairs that are universally accessible to branches of
       * the route tree.
       *
       * @type {?Object}
       */
      queryParams: {
        type: Object,
        value: null,
        notify: true
      },

      /**
       * The serialized path through the route tree. This corresponds to the
       * `window.location.pathname` value, and will update to reflect changes
       * to that value.
       */
      path: {
        type: String,
        notify: true,
        value: ''
      }
    },

    observers: [
      '_locationChanged(path, queryParams)',
      '_routeChanged(route.prefix, route.path)',
      '_routeQueryParamsChanged(route.__queryParams)'
    ],

    created: function() {
      this.linkPaths('route.__queryParams', 'queryParams');
      this.linkPaths('queryParams', 'route.__queryParams');
    },

    /**
     * Handler called when the path or queryParams change.
     *
     * @param  {!string} path The serialized path through the route tree.
     * @param  {Object} queryParams A set of key/value pairs that are
     * universally accessible to branches of the route tree.
     */
    _locationChanged: function(path, queryParams) {
      this.route = {
        prefix: '',
        path: path,
        __queryParams: queryParams
      };
    },

    /**
     * Handler called when the route prefix and route path change.
     *
     * @param  {!string} prefix The fragment of the pathname that precedes the
     * path.
     * @param  {!string} path The serialized path through the route tree.
     */
    _routeChanged: function(prefix, path) {
      if (!this.route) {
        return;
      }

      this.path = prefix + path;
    },

    /**
     * Handler called when the route queryParams change.
     *
     * @param  {Object} queryParams A set of key/value pairs that are
     * universally accessible to branches of the route tree.
     */
    _routeQueryParamsChanged: function(queryParams) {
      if (!this.route) {
        return;
      }
      this.queryParams = queryParams;
    }
  });