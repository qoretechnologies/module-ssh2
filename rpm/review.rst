SSH2 RPM qualification review
=============================

Copyright 2026 Qore Technologies, s.r.o.

Scope: portable RPM recipe, private OpenSSH fixture, explicit test URI options,
strict documentation and repaired API references. QPP edits are documentation
comments only; no native runtime implementation changes.

Validation: the initial RPM candidate built on Fedora 44, Leap 16.0 and EL10.
All eight installed suites passed after the named URI / PAM fixture correction
on each target. The final source overlay rebuilt on Leap with strict Doxygen:
all five API references pass, seven Python fixture tests pass, and the eight
SSH2 suites execute 69 cases with 676 assertions against native/AOT artifacts.
Logs: qore-packaging/results/ssh2-docs-5.log and
{fedora,leap,el10}-ssh2-pam-3.log. Final committed RPMs and minimal runtime
qualification remain separate gates; this review does not claim their success.

.. list-table:: Full audit checklist
   :header-rows: 1

   * - Check
     - Status
     - Evidence

   * - 1. Entry exists in doxygen/lang/120_modules.dox.tmpl (for modules in the Qore repo; N/A for external module repos)
     - N/A
     - No new module/class, DataProvider action/type/registration or Java dependency; documentation-only provider edits do not change those contracts.

   * - 2. Entry exists in doxygen/lang/900_release_notes.dox.tmpl (for modules in the Qore repo; external modules have release notes in their .qm)
     - N/A
     - No new module/class, DataProvider action/type/registration or Java dependency; documentation-only provider edits do not change those contracts.

   * - 3. qore_user_module() or qore_external_user_module() call in CMakeLists.txt
     - N/A
     - No new module/class, DataProvider action/type/registration or Java dependency; documentation-only provider edits do not change those contracts.

   * - 4. Module added to QMOD list in CMakeLists.txt
     - N/A
     - No new module/class, DataProvider action/type/registration or Java dependency; documentation-only provider edits do not change those contracts.

   * - 5. .qm file has @section <lowercasemodname>intro as first doc section — must be all lowercase (e.g., avrodataproviderintro, not AvroDataProviderintro)
     - N/A
     - No new module/class, DataProvider action/type/registration or Java dependency; documentation-only provider edits do not change those contracts.

   * - 6. %modern in .qm file — no redundant %new-style, %require-types, %strict-args, %enable-all-warnings
     - N/A
     - No new module/class, DataProvider action/type/registration or Java dependency; documentation-only provider edits do not change those contracts.

   * - 7. No parse directives (%requires, %modern, %new-style) in separated .qc files (check OUTSIDE of @code blocks only)
     - N/A
     - No new module/class, DataProvider action/type/registration or Java dependency; documentation-only provider edits do not change those contracts.

   * - 8. No %include usage (deprecated for modules)
     - N/A
     - No new module/class, DataProvider action/type/registration or Java dependency; documentation-only provider edits do not change those contracts.

   * - 9. Copyright 2026 on all new files
     - Pass
     - New runner/tests/docs carry 2026 notices; touched Qore/QPP headers also identify 2026.

   * - 10. Directory layout: .qm inside qlib/<ModuleName>/ directory (not at qlib/<ModuleName>.qm for multi-file modules)
     - N/A
     - No new module/class, DataProvider action/type/registration or Java dependency; documentation-only provider edits do not change those contracts.

   * - 11. No second .qm for the same module at qlib/<ModuleName>.qm
     - N/A
     - No new module/class, DataProvider action/type/registration or Java dependency; documentation-only provider edits do not change those contracts.

   * - 12. ns=Qore::XX matches the QoreNamespace constructor path
     - N/A
     - No new module/class, DataProvider action/type/registration or Java dependency; documentation-only provider edits do not change those contracts.

   * - 13. %modern directive present
     - Pass
     - Changed Qore tests retain %modern.

   * - 14. Executable permission set (chmod +x)
     - Pass
     - All seven changed qtest files retain executable mode.

   * - 15. Uses %prepend-module-path  before %requires for in-repo modules (Qore and Qore modules only; not Qorus)
     - Pass
     - Development suites retain their local qlib prepend. The packaging runner copies only tests and removes that prepend, preloading all five exact built/installed qmods.

   * - 16. External module dependencies use %try-module — except modules delivered with the project itself (Qore ex: DataProvider, ConnectionProvider, QUnit, etc.) which use hard %requires
     - Pass
     - No new Qore module import; own SSH2 components are required, while standard Qore modules remain platform dependencies.

   * - 17. No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks
     - N/A
     - No new module/class, DataProvider action/type/registration or Java dependency; documentation-only provider edits do not change those contracts.

   * - 18. No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks
     - N/A
     - No new module/class, DataProvider action/type/registration or Java dependency; documentation-only provider edits do not change those contracts.

   * - 19. If filesystem/network ops exist, verify QoreSandboxManagerHelper usage
     - N/A
     - No new module/class, DataProvider action/type/registration or Java dependency; documentation-only provider edits do not change those contracts.

   * - 20. No File::, Dir::, Socket::, HTTPClient:: usage without justification
     - Pass
     - Network and filesystem activity is confined to the explicit SSH integration fixture: loopback, temporary keys, home and account lookup files.

   * - 21. All for/while loops that could iterate >100 times have qore_check_cancel() checks
     - N/A
     - No new module/class, DataProvider action/type/registration or Java dependency; documentation-only provider edits do not change those contracts.

   * - 22. Uses qore_check_cancel() (NOT deprecated qore_check_io_interrupt())
     - N/A
     - No new module/class, DataProvider action/type/registration or Java dependency; documentation-only provider edits do not change those contracts.

   * - 23. Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations
     - N/A
     - No new module/class, DataProvider action/type/registration or Java dependency; documentation-only provider edits do not change those contracts.

   * - 24. No blocking operations without cancellation support
     - Pass
     - Server readiness uses a listening-log event and monotonic deadline; continuous log draining prevents pipe deadlock. Process-group termination/reaping runs on success, startup failure and test exceptions.

   * - 25. Every action has display_name, short_desc (plain text, <80 chars), desc (markdown)
     - N/A
     - No new module/class, DataProvider action/type/registration or Java dependency; documentation-only provider edits do not change those contracts.

   * - 26. Every action has options populated via getActionOptionFromFields() — without this, the action shows an empty, unusable form
     - N/A
     - No new module/class, DataProvider action/type/registration or Java dependency; documentation-only provider edits do not change those contracts.

   * - 27. Every action has output_type set to a typed data type constant (e.g., MyResponseDataType) — not omitted
     - N/A
     - No new module/class, DataProvider action/type/registration or Java dependency; documentation-only provider edits do not change those contracts.

   * - 28. DPAT_API actions: provider has "supports_request": True and implements doRequestImpl()
     - N/A
     - No new module/class, DataProvider action/type/registration or Java dependency; documentation-only provider edits do not change those contracts.

   * - 29. DPAT_FIND actions: every option exists in SearchOptions, getRecordTypeImpl() returns *hash<string, AbstractDataField>
     - N/A
     - No new module/class, DataProvider action/type/registration or Java dependency; documentation-only provider edits do not change those contracts.

   * - 30. Scheme-based apps (with "scheme" in registerApp): actions use "path" and do NOT use "cls" — having both scheme and cls causes a runtime error
     - N/A
     - No new module/class, DataProvider action/type/registration or Java dependency; documentation-only provider edits do not change those contracts.

   * - 31. Single-key hash slices use trailing comma: Fields{"key",} (without trailing comma, Fields{"key"} returns the value, not a hash)
     - N/A
     - No new module/class, DataProvider action/type/registration or Java dependency; documentation-only provider edits do not change those contracts.

   * - 32. Typed data type classes exist for request and response types — inherit HashDataType, have const Fields hash, call addQoreFields(Fields) in constructor, export public constant at bottom (e.g., public const MyDataType = new MyDataType();)
     - N/A
     - No new module/class, DataProvider action/type/registration or Java dependency; documentation-only provider edits do not change those contracts.

   * - 33. Request/input types use public Fields (enables ClassName::Fields in action registration)
     - N/A
     - No new module/class, DataProvider action/type/registration or Java dependency; documentation-only provider edits do not change those contracts.

   * - 34. Response/output types use private Fields
     - N/A
     - No new module/class, DataProvider action/type/registration or Java dependency; documentation-only provider edits do not change those contracts.

   * - 35. Each field in data types has display_name, type, and desc (markdown-formatted)
     - N/A
     - No new module/class, DataProvider action/type/registration or Java dependency; documentation-only provider edits do not change those contracts.

   * - 36. Input fields have example_value where useful (string fields, endpoint URIs, SQL queries, etc.)
     - N/A
     - No new module/class, DataProvider action/type/registration or Java dependency; documentation-only provider edits do not change those contracts.

   * - 37. Fields with finite allowed values use allowed_values with AllowedValueInfo containing both value and display_name (Title Case, human-readable) — never bare values, never described only in text
     - N/A
     - No new module/class, DataProvider action/type/registration or Java dependency; documentation-only provider edits do not change those contracts.

   * - 38. Password/secret fields have "sensitive": True
     - N/A
     - No new module/class, DataProvider action/type/registration or Java dependency; documentation-only provider edits do not change those contracts.

   * - 39. groups uses AppGroup enum values from qlib/DataProvider/AppGroup.qc
     - N/A
     - No new module/class, DataProvider action/type/registration or Java dependency; documentation-only provider edits do not change those contracts.

   * - 40. App logo stored as separate file, loaded at module level in Priv namespace
     - N/A
     - No new module/class, DataProvider action/type/registration or Java dependency; documentation-only provider edits do not change those contracts.

   * - 41. App desc uses markdown: bullet list of capabilities, links to project website, business-language explanation of value
     - N/A
     - No new module/class, DataProvider action/type/registration or Java dependency; documentation-only provider edits do not change those contracts.

   * - 42. display_name is user-friendly ("Apache Avro" not "avro")
     - N/A
     - No new module/class, DataProvider action/type/registration or Java dependency; documentation-only provider edits do not change those contracts.

   * - 43. short_desc is plain text, under 80 chars, single sentence — no markdown
     - N/A
     - No new module/class, DataProvider action/type/registration or Java dependency; documentation-only provider edits do not change those contracts.

   * - 44. desc uses markdown: backticks for code/field refs ( field_name ,  True ,  pdf ), \n\n for paragraphs, -  bullet lists for enumerations, bold for caveats
     - N/A
     - No new module/class, DataProvider action/type/registration or Java dependency; documentation-only provider edits do not change those contracts.

   * - 45. Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"
     - N/A
     - No new module/class, DataProvider action/type/registration or Java dependency; documentation-only provider edits do not change those contracts.

   * - 46. No bare True/False/NOTHING — must be backtick-wrapped in desc
     - N/A
     - No new module/class, DataProvider action/type/registration or Java dependency; documentation-only provider edits do not change those contracts.

   * - 47. No bare field/option names in prose — must use backticks
     - N/A
     - No new module/class, DataProvider action/type/registration or Java dependency; documentation-only provider edits do not change those contracts.

   * - 48. Long descriptions (>500 chars) use bold section headers and bullet lists
     - N/A
     - No new module/class, DataProvider action/type/registration or Java dependency; documentation-only provider edits do not change those contracts.

   * - 49. Factory registration in Qore repo: every factory name registered in qlib/DataProvider/DataProvider.qc → FactoryMap (without this, module loads but doesn't appear in Qorus apps)
     - N/A
     - No new module/class, DataProvider action/type/registration or Java dependency; documentation-only provider edits do not change those contracts.

   * - 50. getRecordTypeImpl() signature: must be private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) — NOT returning *AbstractDataProviderType
     - N/A
     - No new module/class, DataProvider action/type/registration or Java dependency; documentation-only provider edits do not change those contracts.

   * - 51. Dependency JARs committed (for JNI modules): JAR files in qlib/*/jar/ may be gitignored — use git add -f to ensure they're tracked, otherwise CI compilation fails
     - N/A
     - No new module/class, DataProvider action/type/registration or Java dependency; documentation-only provider edits do not change those contracts.

   * - 52. JAR install rules in CMakeLists.txt for all dependency JARs
     - N/A
     - No new module/class, DataProvider action/type/registration or Java dependency; documentation-only provider edits do not change those contracts.

   * - 53. No workarounds: No TODOs, FIXMEs, stubs, or partially-implemented features
     - Pass
     - Declares actual dependencies and complete tests; no disabled warning checks. The private server disables per-source penalties solely to exercise required negative authentication tests.

   * - 54. Exception safety: C++ uses ReferenceHolder for Qore allocations, std::unique_ptr for C++ allocations, *xsink checked after every fallible operation
     - Pass
     - Context managers own temporary files, pipes, the log reader and server. Seven fixture tests cover EOF, deadlines, cleanup, log pressure, kill escalation and missing compiled modules.

   * - 55. Thread safety: All mutable shared state protected by std::lock_guard<std::mutex> or documented as immutable-after-construction
     - Pass
     - Only the fixture log-reader thread is new; it exclusively drains its pipe, and teardown collects it after stopping the process group.

   * - 56. Type safety: Strongly-typed code<return(args)> instead of untyped code; static_cast instead of C casts; typed hashdecls for results; enums where appropriate
     - Pass
     - URI options use QUnit string option types; subprocess argument lists avoid shell interpolation.

   * - 57. Performance: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply
     - Pass
     - One server per suite run, continuously drained; documentation reuses standard SDK tag indexes.

   * - 58. Error handling: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable
     - Pass
     - All eight suites include invalid authentication cases; fixture negative cases fail explicitly and preserve teardown.

   * - 59. Documentation: Doxygen @param, @return, @throw on all public methods; @par Example with realistic business scenarios; @note for important caveats
     - Pass
     - README and rpm/README.rst explain named URIs, source/build commands, private server behavior and installed checks; strict Doxygen verifies corrected references and parameter names.

   * - 60. QPP flags: [flags=CONSTANT] on methods that never throw; [flags=RET_VALUE_ONLY] on methods that throw but have no side effects
     - Pass
     - Only QPP comments changed; flags and native semantics are unchanged.

   * - 61. Security: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code
     - Pass
     - Ephemeral private keys and passwd/group snapshots; no user SSH configuration or credentials modified. No repository key material is shipped.

   * - 62. Correctness: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)
     - Pass
     - All 69 SSH2 cases / 676 assertions pass with explicit endpoints and compiled modules; all seven fixture tests and five strict API references pass.
