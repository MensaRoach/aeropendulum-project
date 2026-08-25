# 25/08/2026

- Added documents and plans to .docs folder.

## Answers to Phase 0 Questions

- Peripherals are owned by the drivers. If another device will use the same peripheral the main code will first call their init and deinit etc.
- The drivers will be flexible. We will start with blocking and then to nonblocking.
- The sensors will be struct/class objects. They will contain every function related to themselves. Whether peripherals editing their peripherals or doing scaling etc.
- Will be decided on the way.
- Everything will return fail or success.