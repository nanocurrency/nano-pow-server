/** A vanilla es6 router */
export default class Router {
    constructor(root) {
        if (!root) {
            root = window.location.protocol + "//" + window.location.host;
        }
        this.root = null;
        this._routes = [];
        this._paused = false;
        this._destroyed = false;
        this._lastRouteResolved = null;
        this._notFoundHandler = null;
        this._defaultHandler = null;
        this._onLocationChange = this._onLocationChange.bind(this);
        this._genericHooks = null;
        this._historyAPIUpdateMethod = 'pushState';

        if (root) {
            this.root = root.replace(/\/$/, '');
        }

        this._listen();
        this.updatePageLinks();
    }

    navigate(path, absolute) {
        var to;

        path = path || '';
        to = (!absolute ? this._getRoot() + '/' : '') + path.replace(/^\/+/, '/');
        to = to.replace(/([^:])(\/{2,})/g, '$1/');
        history[this._historyAPIUpdateMethod]({}, '', to);
        this.resolve();

        return this;
    }

    on(...args) {
        if (typeof args[0] === 'function') {
            this._defaultHandler = {
                handler: args[0],
                hooks: args[1]
            };
        } else if (args.length >= 2) {
            if (args[0] === '/') {
                let func = args[1];

                if (typeof args[1] === 'object') {
                    func = args[1].uses;
                }

                this._defaultHandler = {
                    handler: func,
                    hooks: args[2]
                };
            } else {
                this._add(args[0], args[1], args[2]);
            }
        } else if (typeof args[0] === 'object') {
            let orderedRoutes = Object.keys(args[0]).sort(compareUrlDepth);

            orderedRoutes.forEach(route => {
                this.on(route, args[0][route]);
            });
        }
        return this;
    }
    off(handler) {
        if (this._defaultHandler !== null && handler === this._defaultHandler.handler) {
            this._defaultHandler = null;
        } else if (this._notFoundHandler !== null && handler === this._notFoundHandler.handler) {
            this._notFoundHandler = null;
        }
        this._routes = this._routes.reduce((result, r) => {
            if (r.handler !== handler) result.push(r);
            return result;
        }, []);
        return this;
    }
    notFound(handler, hooks) {
        this._notFoundHandler = {
            handler,
            hooks: hooks
        };
        return this;
    }
    resolve(current) {
        var handler, m;
        var url = (current || this._cLoc()).replace(this._getRoot(), '');

        let GETParameters = this.extractGETParameters(current || this._cLoc());
        let onlyURL = this.getOnlyURL(url);

        if (this._paused)
            return false;

        m = this.match(onlyURL, this._routes);

        if (m) {
            this._callLeave();
            this._lastRouteResolved = {
                url: onlyURL,
                query: GETParameters,
                hooks: m.route.hooks,
                params: m.params,
                name: m.route.name
            };
            handler = m.route.handler;
            this.manageHooks(() => {
                this.manageHooks(() => {
                    m.route.route instanceof RegExp ?
                        handler(...(m.match.slice(1, m.match.length))) :
                        handler(m.params, GETParameters);
                }, m.route.hooks, m.params, this._genericHooks);
            }, this._genericHooks, m.params);
            return m;
        } else if (this._defaultHandler && (onlyURL === '' || onlyURL === '/')) {
            this.manageHooks(() => {
                this.manageHooks(() => {
                    this._callLeave();
                    this._lastRouteResolved = {
                        url: onlyURL,
                        query: GETParameters,
                        hooks: this._defaultHandler.hooks
                    };
                    this._defaultHandler.handler(GETParameters);
                }, this._defaultHandler.hooks);
            }, this._genericHooks);
            return true;
        } else if (this._notFoundHandler) {
            this.manageHooks(() => {
                this.manageHooks(() => {
                    this._callLeave();
                    this._lastRouteResolved = {
                        url: onlyURL,
                        query: GETParameters,
                        hooks: this._notFoundHandler.hooks
                    };
                    this._notFoundHandler.handler(GETParameters);
                }, this._notFoundHandler.hooks);
            }, this._genericHooks);
        }
        return false;
    }
    destroy() {
        this._routes = [];
        this._destroyed = true;
        this._lastRouteResolved = null;
        this._genericHooks = null;
        clearTimeout(this._listeningInterval);
        if (typeof window !== 'undefined') {
            window.removeEventListener('popstate', this._onLocationChange);
            window.removeEventListener('hashchange', this._onLocationChange);
        }
    }
    updatePageLinks() {
        var self = this;

        if (typeof document === 'undefined') return;

        this._findLinks().forEach(link => {
            if (!link.hasListenerAttached) {
                link.addEventListener('click', function(e) {
                    if ((e.ctrlKey || e.metaKey) && e.target.tagName.toLowerCase() == 'a') {
                        return false;
                    }
                    var location = self.getLinkPath(link);

                    if (!self._destroyed) {
                        e.preventDefault();
                        self.navigate(location.replace(/\/+$/, '').replace(/^\/+/, '/'));
                    }
                });
                link.hasListenerAttached = true;
            }
        });
    }
    generate(name, data = {}) {
        var result = this._routes.reduce((result, route) => {
            var key;

            if (route.name === name) {
                result = route.route;
                for (key in data) {
                    result = result.toString().replace(':' + key, data[key]);
                }
            }
            return result;
        }, '');

        return result;
    }
    link(path) {
        return this._getRoot() + path;
    }
    pause(status = true) {
        this._paused = status;
        if (status) {
            this._historyAPIUpdateMethod = 'replaceState';
        } else {
            this._historyAPIUpdateMethod = 'pushState';
        }
    }
    resume() {
        this.pause(false);
    }
    historyAPIUpdateMethod(value) {
        if (typeof value === 'undefined') return this._historyAPIUpdateMethod;
        this._historyAPIUpdateMethod = value;
        return value;
    }
    disableIfAPINotAvailable() {
        if (!isPushStateAvailable()) {
            this.destroy();
        }
    }
    lastRouteResolved() {
        return this._lastRouteResolved;
    }
    getLinkPath(link) {
        return link.getAttribute('href');
    }
    hooks(hooks) {
        this._genericHooks = hooks;
    }
    _add(route, handler = null, hooks = null) {
        if (typeof route === 'string') {
            route = encodeURI(route);
        }
        this._routes.push(
            typeof handler === 'object' ? {
                route,
                handler: handler.uses,
                name: handler.as,
                hooks: hooks || handler.hooks
            } : {
                route,
                handler,
                hooks: hooks
            }
        );

        return this._add;
    }
    _getRoot() {
        if (this.root !== null) return this.root;
        this.root = this.get_root(this._cLoc().split('?')[0], this._routes);
        return this.root;
    }
    _listen() {
        window.addEventListener('popstate', this._onLocationChange);
    }
    _cLoc() {
        if (typeof window !== 'undefined') {
            if (typeof window.__NAVIGO_WINDOW_LOCATION_MOCK__ !== 'undefined') {
                return window.__NAVIGO_WINDOW_LOCATION_MOCK__;
            }
            return this.clean(window.location.href);
        }
        return '';
    }
    _findLinks() {
        return [].slice.call(document.querySelectorAll('[app-route]'));
    }
    _onLocationChange() {
        this.resolve();
    }
    _callLeave() {
        const lastRouteResolved = this._lastRouteResolved;

        if (lastRouteResolved && lastRouteResolved.hooks && lastRouteResolved.hooks.leave) {
            lastRouteResolved.hooks.leave(lastRouteResolved.params);
        }
    }

    clean(s) {
        if (s instanceof RegExp) return s;
        return s.replace(/\/+$/, '').replace(/^\/+/, '^/');
    }

    regExpResultToParams(match, names) {
        if (names.length === 0) return null;
        if (!match) return null;
        return match
            .slice(1, match.length)
            .reduce((params, value, index) => {
                if (params === null) params = {};
                params[names[index]] = decodeURIComponent(value);
                return params;
            }, null);
    }

    replaceDynamicURLParts(route) {
        var paramNames = [],
            regexp;

        if (route instanceof RegExp) {
            regexp = route;
        } else {
            regexp = new RegExp(
                route.replace(Router.PARAMETER_REGEXP, (full, dots, name) => {
                    paramNames.push(name);
                    return Router.REPLACE_VARIABLE_REGEXP;
                })
                .replace(Router.WILDCARD_REGEXP, Router.REPLACE_WILDCARD) + Router.FOLLOWED_BY_SLASH_REGEXP, Router.MATCH_REGEXP_FLAGS);
        }
        return {
            regexp,
            paramNames
        };
    }

    getUrlDepth(url) {
        return url.replace(/\/$/, '').split('/').length;
    }

    compareUrlDepth(urlA, urlB) {
        return getUrlDepth(urlB) - getUrlDepth(urlA);
    }

    findMatchedRoutes(url, routes = []) {
        return routes
            .map(route => {
                var {
                    regexp,
                    paramNames
                } = this.replaceDynamicURLParts(this.clean(route.route));
                var match = url.replace(/^\/+/, '/').match(regexp);
                var params = this.regExpResultToParams(match, paramNames);

                return match ? {
                    match,
                    route,
                    params
                } : false;
            })
            .filter(m => m);
    }

    match(url, routes) {
        return this.findMatchedRoutes(url, routes)[0] || false;
    }

    get_root(url, routes) {
        var matched = routes.map(
            route => route.route === '' || route.route === '*' ? url : url.split(new RegExp(route.route + '($|\/)'))[0]
        );
        var fallbackURL = clean(url);

        if (matched.length > 1) {
            return matched.reduce((result, url) => {
                if (result.length > url.length) result = url;
                return result;
            }, matched[0]);
        } else if (matched.length === 1) {
            return matched[0];
        }
        return fallbackURL;
    }

    extractGETParameters(url) {
        return url.split(/\?(.*)?$/).slice(1).join('');
    }

    getOnlyURL(url) {
        var onlyURL = url,
            split;
        return url.split(/\?(.*)?$/)[0];
    }

    manageHooks(handler, hooks, params) {
        if (hooks && typeof hooks === 'object') {
            if (hooks.before) {
                hooks.before((shouldRoute = true) => {
                    if (!shouldRoute) return;
                    handler();
                    hooks.after && hooks.after(params);
                }, params);
                return;
            } else if (hooks.after) {
                handler();
                hooks.after && hooks.after(params);
                return;
            }
        }
        handler();
    }

}

Router.PARAMETER_REGEXP = /([:*])(\w+)/g;
Router.WILDCARD_REGEXP = /\*/g;
Router.REPLACE_VARIABLE_REGEXP = '([^\/]+)';
Router.REPLACE_WILDCARD = '(?:.*)';
Router.FOLLOWED_BY_SLASH_REGEXP = '(?:\/$|$)';
Router.MATCH_REGEXP_FLAGS = '';

