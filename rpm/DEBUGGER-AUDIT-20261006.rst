SSH2 RPM debugger compatibility audit
=====================================

Copyright 2026 Qore Technologies, s.r.o.

Scope: release 4 packaging only. Full evidence: qore-packaging/evidence/ssh2-proj-debugger-proposal-20261006.json. User approval: Approve this debugger configuration (2026-10-06).

.. list-table:: Complete audit-changes checklist
   :header-rows: 1

   * - Check
     - Status
     - Evidence

   * - 1. Entry exists in doxygen/lang/120_modules.dox.tmpl (for modules in the Qore repo; N/A for external module repos)
     - N/A
     - RPM/debug metadata changes only; no C++/Qore runtime, new public API, QPP class, DataProvider registration or module layout changes.

   * - 2. Entry exists in doxygen/lang/900_release_notes.dox.tmpl (for modules in the Qore repo; external modules have release notes in their .qm)
     - N/A
     - RPM/debug metadata changes only; no C++/Qore runtime, new public API, QPP class, DataProvider registration or module layout changes.

   * - 3. qore_user_module() or qore_external_user_module() call in CMakeLists.txt
     - N/A
     - RPM/debug metadata changes only; no C++/Qore runtime, new public API, QPP class, DataProvider registration or module layout changes.

   * - 4. Module added to QMOD list in CMakeLists.txt
     - N/A
     - RPM/debug metadata changes only; no C++/Qore runtime, new public API, QPP class, DataProvider registration or module layout changes.

   * - 5. .qm file has @section <lowercasemodname>intro as first doc section — must be all lowercase (e.g., avrodataproviderintro, not AvroDataProviderintro)
     - N/A
     - RPM/debug metadata changes only; no C++/Qore runtime, new public API, QPP class, DataProvider registration or module layout changes.

   * - 6. %modern in .qm file — no redundant %new-style, %require-types, %strict-args, %enable-all-warnings
     - N/A
     - RPM/debug metadata changes only; no C++/Qore runtime, new public API, QPP class, DataProvider registration or module layout changes.

   * - 7. No parse directives (%requires, %modern, %new-style) in separated .qc files (check OUTSIDE of @code blocks only)
     - N/A
     - RPM/debug metadata changes only; no C++/Qore runtime, new public API, QPP class, DataProvider registration or module layout changes.

   * - 8. No %include usage (deprecated for modules)
     - N/A
     - RPM/debug metadata changes only; no C++/Qore runtime, new public API, QPP class, DataProvider registration or module layout changes.

   * - 9. Copyright 2026 on all new files
     - Pass
     - Existing spec and new audit retain 2026 copyright.

   * - 10. Directory layout: .qm inside qlib/<ModuleName>/ directory (not at qlib/<ModuleName>.qm for multi-file modules)
     - N/A
     - RPM/debug metadata changes only; no C++/Qore runtime, new public API, QPP class, DataProvider registration or module layout changes.

   * - 11. No second .qm for the same module at qlib/<ModuleName>.qm
     - N/A
     - RPM/debug metadata changes only; no C++/Qore runtime, new public API, QPP class, DataProvider registration or module layout changes.

   * - 12. ns=Qore::XX matches the QoreNamespace constructor path
     - N/A
     - RPM/debug metadata changes only; no C++/Qore runtime, new public API, QPP class, DataProvider registration or module layout changes.

   * - 13. %modern directive present
     - N/A
     - RPM/debug metadata changes only; no C++/Qore runtime, new public API, QPP class, DataProvider registration or module layout changes.

   * - 14. Executable permission set (chmod +x)
     - N/A
     - RPM/debug metadata changes only; no C++/Qore runtime, new public API, QPP class, DataProvider registration or module layout changes.

   * - 15. Uses %prepend-module-path  before %requires for in-repo modules (Qore and Qore modules only; not Qorus)
     - N/A
     - RPM/debug metadata changes only; no C++/Qore runtime, new public API, QPP class, DataProvider registration or module layout changes.

   * - 16. External module dependencies use %try-module — except modules delivered with the project itself (Qore ex: DataProvider, ConnectionProvider, QUnit, etc.) which use hard %requires
     - N/A
     - RPM/debug metadata changes only; no C++/Qore runtime, new public API, QPP class, DataProvider registration or module layout changes.

   * - 17. No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks
     - N/A
     - RPM/debug metadata changes only; no C++/Qore runtime, new public API, QPP class, DataProvider registration or module layout changes.

   * - 18. No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks
     - N/A
     - RPM/debug metadata changes only; no C++/Qore runtime, new public API, QPP class, DataProvider registration or module layout changes.

   * - 19. If filesystem/network ops exist, verify QoreSandboxManagerHelper usage
     - N/A
     - RPM/debug metadata changes only; no C++/Qore runtime, new public API, QPP class, DataProvider registration or module layout changes.

   * - 20. No File::, Dir::, Socket::, HTTPClient:: usage without justification
     - N/A
     - RPM/debug metadata changes only; no C++/Qore runtime, new public API, QPP class, DataProvider registration or module layout changes.

   * - 21. All for/while loops that could iterate >100 times have qore_check_cancel() checks
     - N/A
     - RPM/debug metadata changes only; no C++/Qore runtime, new public API, QPP class, DataProvider registration or module layout changes.

   * - 22. Uses qore_check_cancel() (NOT deprecated qore_check_io_interrupt())
     - N/A
     - RPM/debug metadata changes only; no C++/Qore runtime, new public API, QPP class, DataProvider registration or module layout changes.

   * - 23. Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations
     - N/A
     - RPM/debug metadata changes only; no C++/Qore runtime, new public API, QPP class, DataProvider registration or module layout changes.

   * - 24. No blocking operations without cancellation support
     - N/A
     - RPM/debug metadata changes only; no C++/Qore runtime, new public API, QPP class, DataProvider registration or module layout changes.

   * - 25. Every action has display_name, short_desc (plain text, <80 chars), desc (markdown)
     - N/A
     - RPM/debug metadata changes only; no C++/Qore runtime, new public API, QPP class, DataProvider registration or module layout changes.

   * - 26. Every action has options populated via getActionOptionFromFields() — without this, the action shows an empty, unusable form
     - N/A
     - RPM/debug metadata changes only; no C++/Qore runtime, new public API, QPP class, DataProvider registration or module layout changes.

   * - 27. Every action has output_type set to a typed data type constant (e.g., MyResponseDataType) — not omitted
     - N/A
     - RPM/debug metadata changes only; no C++/Qore runtime, new public API, QPP class, DataProvider registration or module layout changes.

   * - 28. DPAT_API actions: provider has "supports_request": True and implements doRequestImpl()
     - N/A
     - RPM/debug metadata changes only; no C++/Qore runtime, new public API, QPP class, DataProvider registration or module layout changes.

   * - 29. DPAT_FIND actions: every option exists in SearchOptions, getRecordTypeImpl() returns *hash<string, AbstractDataField>
     - N/A
     - RPM/debug metadata changes only; no C++/Qore runtime, new public API, QPP class, DataProvider registration or module layout changes.

   * - 30. Scheme-based apps (with "scheme" in registerApp): actions use "path" and do NOT use "cls" — having both scheme and cls causes a runtime error
     - N/A
     - RPM/debug metadata changes only; no C++/Qore runtime, new public API, QPP class, DataProvider registration or module layout changes.

   * - 31. Single-key hash slices use trailing comma: Fields{"key",} (without trailing comma, Fields{"key"} returns the value, not a hash)
     - N/A
     - RPM/debug metadata changes only; no C++/Qore runtime, new public API, QPP class, DataProvider registration or module layout changes.

   * - 32. Typed data type classes exist for request and response types — inherit HashDataType, have const Fields hash, call addQoreFields(Fields) in constructor, export public constant at bottom (e.g., public const MyDataType = new MyDataType();)
     - N/A
     - RPM/debug metadata changes only; no C++/Qore runtime, new public API, QPP class, DataProvider registration or module layout changes.

   * - 33. Request/input types use public Fields (enables ClassName::Fields in action registration)
     - N/A
     - RPM/debug metadata changes only; no C++/Qore runtime, new public API, QPP class, DataProvider registration or module layout changes.

   * - 34. Response/output types use private Fields
     - N/A
     - RPM/debug metadata changes only; no C++/Qore runtime, new public API, QPP class, DataProvider registration or module layout changes.

   * - 35. Each field in data types has display_name, type, and desc (markdown-formatted)
     - N/A
     - RPM/debug metadata changes only; no C++/Qore runtime, new public API, QPP class, DataProvider registration or module layout changes.

   * - 36. Input fields have example_value where useful (string fields, endpoint URIs, SQL queries, etc.)
     - N/A
     - RPM/debug metadata changes only; no C++/Qore runtime, new public API, QPP class, DataProvider registration or module layout changes.

   * - 37. Fields with finite allowed values use allowed_values with AllowedValueInfo containing both value and display_name (Title Case, human-readable) — never bare values, never described only in text
     - N/A
     - RPM/debug metadata changes only; no C++/Qore runtime, new public API, QPP class, DataProvider registration or module layout changes.

   * - 38. Password/secret fields have "sensitive": True
     - N/A
     - RPM/debug metadata changes only; no C++/Qore runtime, new public API, QPP class, DataProvider registration or module layout changes.

   * - 39. groups uses AppGroup enum values from qlib/DataProvider/AppGroup.qc
     - N/A
     - RPM/debug metadata changes only; no C++/Qore runtime, new public API, QPP class, DataProvider registration or module layout changes.

   * - 40. App logo stored as separate file, loaded at module level in Priv namespace
     - N/A
     - RPM/debug metadata changes only; no C++/Qore runtime, new public API, QPP class, DataProvider registration or module layout changes.

   * - 41. App desc uses markdown: bullet list of capabilities, links to project website, business-language explanation of value
     - N/A
     - RPM/debug metadata changes only; no C++/Qore runtime, new public API, QPP class, DataProvider registration or module layout changes.

   * - 42. display_name is user-friendly ("Apache Avro" not "avro")
     - N/A
     - RPM/debug metadata changes only; no C++/Qore runtime, new public API, QPP class, DataProvider registration or module layout changes.

   * - 43. short_desc is plain text, under 80 chars, single sentence — no markdown
     - N/A
     - RPM/debug metadata changes only; no C++/Qore runtime, new public API, QPP class, DataProvider registration or module layout changes.

   * - 44. desc uses markdown: backticks for code/field refs ( field_name ,  True ,  pdf ), \n\n for paragraphs, -  bullet lists for enumerations, bold for caveats
     - N/A
     - RPM/debug metadata changes only; no C++/Qore runtime, new public API, QPP class, DataProvider registration or module layout changes.

   * - 45. Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"
     - N/A
     - RPM/debug metadata changes only; no C++/Qore runtime, new public API, QPP class, DataProvider registration or module layout changes.

   * - 46. No bare True/False/NOTHING — must be backtick-wrapped in desc
     - N/A
     - RPM/debug metadata changes only; no C++/Qore runtime, new public API, QPP class, DataProvider registration or module layout changes.

   * - 47. No bare field/option names in prose — must use backticks
     - N/A
     - RPM/debug metadata changes only; no C++/Qore runtime, new public API, QPP class, DataProvider registration or module layout changes.

   * - 48. Long descriptions (>500 chars) use bold section headers and bullet lists
     - N/A
     - RPM/debug metadata changes only; no C++/Qore runtime, new public API, QPP class, DataProvider registration or module layout changes.

   * - 49. Factory registration in Qore repo: every factory name registered in qlib/DataProvider/DataProvider.qc → FactoryMap (without this, module loads but doesn't appear in Qorus apps)
     - N/A
     - RPM/debug metadata changes only; no C++/Qore runtime, new public API, QPP class, DataProvider registration or module layout changes.

   * - 50. getRecordTypeImpl() signature: must be private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) — NOT returning *AbstractDataProviderType
     - N/A
     - RPM/debug metadata changes only; no C++/Qore runtime, new public API, QPP class, DataProvider registration or module layout changes.

   * - 51. Dependency JARs committed (for JNI modules): JAR files in qlib/*/jar/ may be gitignored — use git add -f to ensure they're tracked, otherwise CI compilation fails
     - N/A
     - RPM/debug metadata changes only; no C++/Qore runtime, new public API, QPP class, DataProvider registration or module layout changes.

   * - 52. JAR install rules in CMakeLists.txt for all dependency JARs
     - N/A
     - RPM/debug metadata changes only; no C++/Qore runtime, new public API, QPP class, DataProvider registration or module layout changes.

   * - 53. No workarounds: No TODOs, FIXMEs, stubs, or partially-implemented features
     - Pass
     - Exact debugger configuration approved; no diagnostics hidden or functionality disabled. Four explicit AOT modules only.

   * - 54. Exception safety: C++ uses ReferenceHolder for Qore allocations, std::unique_ptr for C++ allocations, *xsink checked after every fallible operation
     - Pass
     - Existing Qore helper preserves appended metadata around ELF edits; failure propagates through RPM.

   * - 55. Thread safety: All mutable shared state protected by std::lock_guard<std::mutex> or documented as immutable-after-construction
     - N/A
     - RPM/debug metadata changes only; no C++/Qore runtime, new public API, QPP class, DataProvider registration or module layout changes.

   * - 56. Type safety: Strongly-typed code<return(args)> instead of untyped code; static_cast instead of C casts; typed hashdecls for results; enums where appropriate
     - Pass
     - Explicit fixed module paths and SHA256-pinned paired/proposal artifacts.

   * - 57. Performance: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply
     - Pass
     - Only optional accelerator omitted; source and full DWARF retained. Initial debugger loading may be slower.

   * - 58. Error handling: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable
     - Pass
     - Each package build asserts QAMD metadata and separate debug links; GDB checks require resolvable source and breakpoints.

   * - 59. Documentation: Doxygen @param, @return, @throw on all public methods; @par Example with realistic business scenarios; @note for important caveats
     - Pass
     - RPM README and release 4 changelog describe the configuration and tradeoff.

   * - 60. QPP flags: [flags=CONSTANT] on methods that never throw; [flags=RET_VALUE_ONLY] on methods that throw but have no side effects
     - N/A
     - RPM/debug metadata changes only; no C++/Qore runtime, new public API, QPP class, DataProvider registration or module layout changes.

   * - 61. Security: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code
     - Pass
     - All normal compiler flags, signing and package checks unchanged; no user-supplied executable text.

   * - 62. Correctness: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)
     - Pass
     - Three proposal RPMs pass 69 cases each; 12 SSH2 paired controls preserve every other DWARF section and all symbols; 12 resulting RPM debugger checks pass without diagnostics.
