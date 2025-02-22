//config CONFIG_SSL_SERVER_ONLY
//    bool "Server only - no verification"
//    help
//        Enable server functionality (no client functionality).
//        This mode still supports sessions and chaining (which can be turned
//        off in configuration).
//
//        The axssl sample runs with the minimum of features.
//
//        This is the most space efficient of the modes with the library
//        about 45kB in size. Use this mode if you are doing standard SSL server
//        work.

//config CONFIG_SSL_CERT_VERIFICATION
//    bool "Server only - with verification"
//    help
//        Enable server functionality with client authentication (no client
//        functionality).
//
//        The axssl sample runs with the "-verify" and "-CAfile" options.
//
//        This mode produces a library about 49kB in size. Use this mode if you
//        have an SSL server which requires client authentication (which is
//        uncommon in browser applications).

//config CONFIG_SSL_ENABLE_CLIENT
//    bool "Client/Server enabled"
//    help
//        Enable client/server functionality (including peer authentication).
//
//        The axssl sample runs with the "s_client" option enabled.
//
//        This mode produces a library about 51kB in size. Use this mode if you
//        require axTLS to use SSL client functionality (the SSL server code
//        is always enabled).

#define CONFIG_SSL_FULL_MODE

//config CONFIG_SSL_FULL_MODE
//    bool "Client/Server enabled with diagnostics"
//    help
//        Enable client/server functionality including diagnostics. Most of the
//        extra size in this mode is due to the storage of various strings that
//        are used.
//
//        The axssl sample has 3 more options, "-debug", "-state" and "-show-rsa"
//
//        This mode produces a library about 58kB in size. It is suggested that
//        this mode is used only during development, or systems that have more
//        generous memory limits.
//
//        It is the default to demonstrate the features of axTLS.

//config CONFIG_SSL_SKELETON_MODE
//    bool "Skeleton mode - the smallest server mode"
//    help
//        This is an experiment to build the smallest library at the expense of
//        features and speed.
//
//        * Server mode only.
//        * The AES cipher is disabled.
//        * No session resumption.
//        * No external keys/certificates are supported.
//        * The bigint library has most of the performance features disabled.
//        * Some other features/API calls may not work.
//
//        This mode produces a library about 37kB in size. The main
//        disadvantage of this mode is speed - it will be much slower than the
//        other build modes.


//choice
//    prompt "Protocol Preference"
//    depends on !CONFIG_SSL_SKELETON_MODE
//    default CONFIG_SSL_PROT_MEDIUM
//
//config CONFIG_SSL_PROT_LOW
//    bool "Low"
//    help
//        Chooses the cipher in the order of RC4-SHA, AES128-SHA, AES256-SHA.
//
//        This will use the fastest cipher(s) but at the expense of security.
//

#define CONFIG_SSL_PROT_MEDIUM 1
//config CONFIG_SSL_PROT_MEDIUM
//    bool "Medium"
//    help
//        Chooses the cipher in the order of AES128-SHA, AES256-SHA, RC4-SHA.
//
//        This mode is a balance between speed and security and is the default.
//
//config CONFIG_SSL_PROT_HIGH
//    bool "High"
//    help
//        Chooses the cipher in the order of AES256-SHA, AES128-SHA, RC4-SHA.
//
//        This will use the strongest cipher(s) at the cost of speed.

#define CONFIG_SSL_USE_DEFAULT_KEY 1
//config CONFIG_SSL_USE_DEFAULT_KEY
//    bool "Enable default key"
//    depends on !CONFIG_SSL_SKELETON_MODE
//    default y
//    help
//        Some applications will not require the default private key/certificate
//        that is built in. This is one way to save on a couple of kB's if an
//        external private key/certificate is used.
//
//        The private key is in ssl/private_key.h and the certificate is in
//        ssl/cert.h.
//
//        The advantage of a built-in private key/certificate is that no file
//        system is required for access. Both the certificate and the private
//        key will be automatically loaded on a ssl_ctx_new().
//
//        However this private key/certificate can never be changed (without a
//        code update).
//
//        This mode is enabled by default. Disable this mode if the
//        built-in key/certificate is not used.
//
#define CONFIG_SSL_PRIVATE_KEY_LOCATION "" //"axTLS.key"
//config CONFIG_SSL_PRIVATE_KEY_LOCATION
//    string "Private key file location"
//    depends on !CONFIG_SSL_USE_DEFAULT_KEY && !CONFIG_SSL_SKELETON_MODE
//    help
//        The file location of the private key which will be automatically
//        loaded on a ssl_ctx_new().
//
#define CONFIG_SSL_PRIVATE_KEY_PASSWORD ""
//config CONFIG_SSL_PRIVATE_KEY_PASSWORD
//    string "Private key password"
//    depends on !CONFIG_SSL_USE_DEFAULT_KEY && CONFIG_SSL_HAS_PEM
//    help
//        The password required to decrypt a PEM-encoded password file.
//
#define CONFIG_SSL_X509_CERT_LOCATION "" //"axTLS_x509.cer"
//config CONFIG_SSL_X509_CERT_LOCATION
//    string "X.509 certificate file location"
//    depends on !CONFIG_SSL_GENERATE_X509_CERT && !CONFIG_SSL_USE_DEFAULT_KEY && !CONFIG_SSL_SKELETON_MODE
//    help
//        The file location of the X.509 certificate which will be automatically
//        loaded on a ssl_ctx_new().
//
//config CONFIG_SSL_GENERATE_X509_CERT
//    bool "Generate X.509 Certificate"
//    default n
//    help
//        An X.509 certificate can be automatically generated on a
//        ssl_ctx_new(). A private key still needs to be provided (the private
//        key in ss/private_key.h will be used unless
//        CONFIG_SSL_PRIVATE_KEY_LOCATION is set).
//
//        The certificate is generated on the fly, and so a minor start-up time
//        penalty is to be expected. This feature adds around 5kB to the
//        library.
//
//        This feature is disabled by default.
//
//config CONFIG_SSL_X509_COMMON_NAME
//    string "X.509 Common Name"
//    depends on CONFIG_SSL_GENERATE_X509_CERT
//    help
//        The common name for the X.509 certificate. This should be the fully
//        qualified domain name (FQDN), e.g. www.foo.com.
//
//        If this is blank, then this will be value from gethostname() and
//        getdomainname().
//
//config CONFIG_SSL_X509_ORGANIZATION_NAME
//    string "X.509 Organization Name"
//    depends on CONFIG_SSL_GENERATE_X509_CERT
//    help
//        The organization name for the generated X.509 certificate.
//
//        This field is optional.
//
//config CONFIG_SSL_X509_ORGANIZATION_UNIT_NAME
//    string "X.509 Organization Unit Name"
//    depends on CONFIG_SSL_GENERATE_X509_CERT
//    help
//        The organization unit name for the generated X.509 certificate.
//
//        This field is optional.

//config CONFIG_SSL_ENABLE_V23_HANDSHAKE
//    bool "Enable v23 Handshake"
//    default y
//    help
//        Some browsers use the v23 handshake client hello message
//        (an SSL2 format message which all SSL servers can understand).
//        It may be used if SSL2 is enabled in the browser.
//
//        Since this feature takes a kB or so, this feature may be disabled - at
//        the risk of making it incompatible with some browsers (IE6 is ok,
//        Firefox 1.5 and below use it).
//
//        Disable if backwards compatibility is not an issue (i.e. the client is
//        always using TLS1.0)

#define CONFIG_SSL_HAS_PEM 1
//config CONFIG_SSL_HAS_PEM
//    bool "Enable PEM"
//    default n if !CONFIG_SSL_FULL_MODE
//    default y if CONFIG_SSL_FULL_MODE
//    depends on !CONFIG_SSL_SKELETON_MODE
//    help
//        Enable the use of PEM format for certificates and private keys.
//
//        PEM is not normally needed - PEM files can be converted into DER files
//        quite easily. However they have the convenience of allowing multiple
//        certificates/keys in the same file.
//
//        This feature will add a couple of kB to the library.
//
//        Disable if PEM is not used (which will be in most cases).

#define CONFIG_SSL_USE_PKCS12 1
//config CONFIG_SSL_USE_PKCS12
//    bool "Use PKCS8/PKCS12"
//    default n if !CONFIG_SSL_FULL_MODE
//    default y if CONFIG_SSL_FULL_MODE
//    depends on !CONFIG_SSL_SERVER_ONLY && !CONFIG_SSL_SKELETON_MODE
//    help
//        PKCS#12 certificates combine private keys and certificates together in
//        one file.
//
//        PKCS#8 private keys are also suppported (as it is a subset of PKCS#12).
//
//        The decryption of these certificates uses RC4-128 (and these
//        certificates must be encrypted using this cipher). The actual
//        algorithm is "PBE-SHA1-RC4-128".
//
//        Disable if PKCS#12 is not used (which will be in most cases).

#define CONFIG_SSL_EXPIRY_TIME 24
//config CONFIG_SSL_EXPIRY_TIME
//    int "Session expiry time (in hours)"
//    depends on !CONFIG_SSL_SKELETON_MODE
//    default 24
//    help
//        The time (in hours) before a session expires.
//
//        A longer time means that the expensive parts of a handshake don't
//        need to be run when a client reconnects later.
//
//        The default is 1 day.
//
#define CONFIG_X509_MAX_CA_CERTS 4
//config CONFIG_X509_MAX_CA_CERTS
//    int "Maximum number of certificate authorites"
//    default 4
//    depends on !CONFIG_SSL_SERVER_ONLY && !CONFIG_SSL_SKELETON_MODE
//    help
//        Determines the number of CA's allowed.
//
//        Increase this figure if more trusted sites are allowed. Each
//        certificate adds about 300 bytes (when added).
//
//        The default is to allow four certification authorities.
//
#define CONFIG_SSL_MAX_CERTS 2
//config CONFIG_SSL_MAX_CERTS
//    int "Maximum number of chained certificates"
//    default 2
//    help
//        Determines the number of certificates used in a certificate
//        chain. The chain length must be at least 1.
//
//        Increase this figure if more certificates are to be added to the
//        chain. Each certificate adds about 300 bytes (when added).
//
//        The default is to allow one certificate + 1 certificate in the chain
//        (which may be the certificate authority certificate).
//
//config CONFIG_SSL_CTX_MUTEXING
//    bool "Enable SSL_CTX mutexing"
//    default n
//    help
//        Normally mutexing is not required - each SSL_CTX object can deal with
//        many SSL objects (as long as each SSL_CTX object is using a single
//        thread).
//
//        If the SSL_CTX object is not thread safe e.g. the case where a
//        new thread is created for each SSL object, then mutexing is required.
//
//        Select y when a mutex on the SSL_CTX object is required.
//
//config CONFIG_USE_DEV_URANDOM
//    bool "Use /dev/urandom"
//    default y
//    depends on !CONFIG_PLATFORM_WIN32
//    help
//        Use /dev/urandom. Otherwise a custom RNG is used.
//
//        This will be the default on most Linux systems.
//
//config CONFIG_WIN32_USE_CRYPTO_LIB
//    bool "Use Win32 Crypto Library"
//    depends on CONFIG_PLATFORM_WIN32
//    help
//        Microsoft produce a Crypto API which requires the Platform SDK to be
//        installed. It's used for the RNG.
//
//        This will be the default on most Win32 systems.
//
//config CONFIG_OPENSSL_COMPATIBLE
//    bool "Enable openssl API compatibility"
//    default n
//    help
//        To ease the porting of openssl applications, a subset of the openssl
//        API is wrapped around the axTLS API.
//
//        Note: not all the API is implemented, so parts may still break. And
//        it's definitely not 100% compatible.
//
//config CONFIG_PERFORMANCE_TESTING
//    bool "Build the bigint performance test tool"
//    default n
//    help
//        Used for performance testing of bigint.
//
//        This is a testing tool and is normally disabled.
//
//config CONFIG_SSL_TEST
//    bool "Build the SSL testing tool"
//    default n
//    depends on CONFIG_SSL_FULL_MODE && !CONFIG_SSL_GENERATE_X509_CERT
//    help
//        Used for sanity checking the SSL handshaking.
//
//        This is a testing tool and is normally disabled.


