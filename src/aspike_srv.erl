% -------------------------------------------------------------------------------

-module(aspike_srv).

-behaviour(gen_server).

% -------------------------------------------------------------------------------

-define(LIBNAME, aspike_port).

-define(DEFAULT_TIMEOUT, 10000).
-define(DEFAULT_HOST, "127.0.0.1").
-define(DEFAULT_PORT, 3010).

-ifndef(TEST).
-define(TEST, true).
-endif.

% -------------------------------------------------------------------------------

% API
-export([
    start_link/0
]).

-ifdef(TEST).
-export([
    start/0
]).
-endif.

-export([
    mk_args/2,
    b/0
]).

% gen_server callbacks
-export([
    init/1,
    handle_call/3,
    handle_cast/2,
    handle_info/2,
    terminate/2
]).

-export([
    aerospike_init/0,
    command/1,
    connect/0,
    connect/2,
    config_info/0,
    cluster_info/0,
    host_add/0,
    host_add/2,
    key_put/0,
    key_put/2,
    key_put/5,
    key_remove/0,
    key_remove/1,
    key_remove/4,
    key_get/0,
    key_get/1,
    key_get/4,
    node_random/0,
    node_names/0,
    node_get/1,
    node_info/2,
    host_info/1,
    host_info/3,
    port_status/0,
    port_info/0,
    help/0,
    help/1,
% ------
    bar/1,
    foo/1]).

-record(state, {
    ext_prg :: string(),
    port :: port()
}).

-type state() :: #state{}.

% -------------------------------------------------------------------------------
%  API
% -------------------------------------------------------------------------------

-spec b() -> ok.
b() ->
    host_add(),
    connect(),
    ok.

% @doc Used in ${tsl.erl} to create argument list for testin function
-spec mk_args(atom(), non_neg_integer()) -> [term()].
mk_args(_, _) -> [].

-ifdef(TEST).
start() ->
    gen_server:start({local, ?MODULE}, ?MODULE, utils:find_lib(?LIBNAME), []).
-endif.

start_link() ->
    gen_server:start_link({local, ?MODULE}, ?MODULE, utils:find_lib(?LIBNAME), []).

-spec command(term()) -> term().
command(Cmd) -> 
    gen_server:call(?MODULE, {command, Cmd}, ?DEFAULT_TIMEOUT + 10).

-spec aerospike_init() -> {ok, string()} | {error, string()}.
aerospike_init() ->
    command({aerospike_init}).

-spec host_add() -> {ok, string()} | {error, string()}.
host_add() ->
    host_add(?DEFAULT_HOST, ?DEFAULT_PORT).

-spec host_add(string(), non_neg_integer()) -> {ok, string()} | {error, string()}.
host_add(Host, Port) when is_list(Host); is_integer(Port) -> 
    command({host_add, Host, Port}).

-spec connect() -> {ok, string()} | {error, string()}.
connect() ->
    connect("", "").

-spec connect(string(), string()) -> {ok, string()} | {error, string()}.
connect(User, Pwd) when is_list(User); is_list(Pwd) -> 
    command({connect, User, Pwd}).

key_put() ->
    key_put("erl-bin-111", 1111).

key_put(Bin, N) ->
    key_put(Bin, N, "test", "erl-set", "erl-key").
    
-spec key_put(string(), integer(), string(), string(), string()) -> {ok, string()} | {error, string()}.
key_put(Bin, N, Namespace, Set, KeyStr) when is_list(Bin); is_integer(N); is_list(Namespace); is_list(Set); is_list(KeyStr) -> 
    command({key_put, Bin, N, Namespace, Set, KeyStr}).

key_remove() ->
    key_remove("erl-bin-111").

key_remove(Bin) ->
    key_remove(Bin, "test", "erl-set", "erl-key").
    
-spec key_remove(string(), string(), string(), string()) -> {ok, string()} | {error, string()}.
key_remove(Bin, Namespace, Set, KeyStr) when is_list(Bin); is_list(Namespace); is_list(Set); is_list(KeyStr) -> 
    command({key_remove, Bin, Namespace, Set, KeyStr}).

key_get() ->
    key_get("erl-bin-111").

key_get(Bin) ->
    key_get(Bin, "test", "erl-set", "erl-key").
    
-spec key_get(string(), string(), string(), string()) -> {ok, string()} | {error, string()}.
key_get(Bin, Namespace, Set, KeyStr) when is_list(Bin); is_list(Namespace); is_list(Set); is_list(KeyStr) -> 
    command({key_get, Bin, Namespace, Set, KeyStr}).
    

-spec config_info() -> {ok, map()} | {error, term()}.
config_info() ->
    command({config_info}).

-spec cluster_info() -> {ok, map()} | {error, term()}.
cluster_info() ->
    command({cluster_info}).

-spec node_random() -> {ok, string()} | {error, term()}.
node_random() ->
    command({node_random}).

-spec node_names() -> {ok, [string()]} | {error, term()}.
node_names() ->
    command({node_names}).

-spec node_get(string()) -> {ok, string()} | {error, term()}.
node_get(NodeName) ->
    command({node_get, NodeName}).

-spec node_info(string(), string()) -> {ok, [string()]} | {error, term()}.
node_info(NodeName, Item) ->
    as_render:info_render(command({node_info, NodeName, Item}), Item).

-spec host_info(string()) -> {ok, [string()]} | {error, string()}.
host_info(Item) ->
    host_info(?DEFAULT_HOST, ?DEFAULT_PORT, Item).

-spec host_info(string(), non_neg_integer(), string()) -> {ok, [string()]} | {error, term()}.
host_info(HostName, Port, Item) ->
    as_render:info_render(command({host_info, HostName, Port, Item}), Item).


-spec port_status() -> {ok, map()} | {error, term()}.
port_status() ->
    command({port_status}).

-spec help() -> {ok, string()} | {error, term()}.
help() -> help("namespaces").

% -spec help("bins" | "sets" | "node" | "namespaces" | "udf-list" | "sindex-list:" | "edition" | "get-config:context=namespace;id=test", "get-config") -> 
% {ok, string()} | {error, term()}.
-spec help(string()) -> {ok, string()} | {error, term()}.
help(Item) ->
    as_render:info_render(command({help, Item}), Item).

-spec port_info() -> [tuple()]| undefined.
port_info() ->
    gen_server:call(?MODULE, port_info).

-spec foo(integer()) -> {ok, integer()} | {error, term()}.
foo(X) when is_integer(X) -> 
    command({foo, X}).

-spec bar(integer()) -> {ok, integer()} | {error, term()}.
% bar(X) when is_integer(X) -> 
bar(X) -> 
    command({bar, X}).

% -------------------------------------------------------------------------------
% Callbacks
% -------------------------------------------------------------------------------

-spec init(string()) -> {ok, state()}.
init(ExtPrg) ->
    process_flag(trap_exit, true),
    Port = open_port({spawn_executable, ExtPrg}, [{packet, 2}, binary, nouse_stdio]),
    spawn(fun aerospike_init/0),
    {ok, #state{ext_prg = ExtPrg, port = Port}}.
    
handle_call({command, Msg}, {Caller, _}, State=#state{port = Port}) ->
    Res = call_port(Caller, Port, Msg),
    {reply, Res, State};
handle_call(port_info, _, State=#state{port = Port}) ->
    Res = erlang:port_info(Port),
    {reply, Res, State};
handle_call(Msg, _From, State) ->
    io:format("~p:~p Msg = ~p~n",[?MODULE, ?FUNCTION_NAME, Msg]),
    {noreply, State}.

handle_cast({command, Msg}, State=#state{port = Port}) ->
    Port ! {self(), {command, term_to_binary(Msg)}},
    {noreply, State};

handle_cast(Msg, State) ->
    io:format("~p:~p Msg = ~p~n",[?MODULE, ?FUNCTION_NAME, Msg]),
    {noreply, State}.

handle_info(Msg, State) ->
    io:format("~p:~p Msg = ~p~n",[?MODULE, ?FUNCTION_NAME, Msg]),
    {noreply, State}.

terminate(_Reason, _State) ->
    ok.

% -------------------------------------------------------------------------------
% helpers
% -------------------------------------------------------------------------------
-spec call_port(pid(), pid(), term()) ->  {ok, term()} | {error, timeout}.
call_port(Caller, Port, Msg) ->
    Port ! {self(), {command, term_to_binary(Msg)}},
    receive
        {_Port, {data, Data}} -> Caller ! binary_to_term(Data)
        after ?DEFAULT_TIMEOUT -> {error, timeout_is_out}
    end.

% -------------------------------------------------------------------------------
    % aql -h 127.0.0.1:3010
    % asadm -e info
    % docker run -d --name aerospike -p 3010-3012:3011-3002 aerospike:ee-7.0.0.3 
    % docker run -d --name aerospike -p 3010-3012:3000-3002 aerospike/aerospike-server-enterprise
    % 
    % make EVENT_LIB=libev
% -------------------------------------------------------------------------------
% 5> aspike_srv:host_add().
% {ok,"host and port added"}
% 6> aspike_srv:connect().
% {ok,"connected"}
% 7> aspike_srv:node_random().
% {ok,"127.0.0.1:3010"}
% 8> aspike_srv:node_names(). 
% {ok,["BB9020011AC4202"]}
% 9> aspike_srv:node_get("BB9020011AC4202").
% {ok,"127.0.0.1:3010"}
% 4> aspike_srv:node_info("BB9020011AC4202", "namespaces").
% {ok,{"namespaces",["test"]}}
% 
% 8> aspike_srv:help("namespaces").
% {ok,{"namespaces",["test"]}}
% 9> aspike_srv:help("nodes").     
% {error,"no data"}
% 10> aspike_srv:help("node"). 
% {ok,"node\tBB9020011AC4202\n"}
% 13> aspike_srv:help("bins").
% 2> aspike_srv:help("sindex-list:").
% {ok,{"sindex-list:",
% [#{"bin" => "binint","context" => null,
%    "indexname" => "page-index","indextype" => "default",
%    "ns" => "test","set" => "queryresume","state" => "RW",
%    "type" => "numeric"}]}}
% 
% 12> tsl:tst(aspike_srv, key_put, 0, 10000).
% aspike_srv:key_put, N=0, R=10000, Time=628.3231
% ok
% 13> tsl:tst(aspike_srv, key_get, 0, 10000).
% aspike_srv:key_get, N=0, R=10000, Time=635.6161
% ok
% -------------------------------------------------------------------------------
