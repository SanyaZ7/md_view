Вот набор тестовых фрагментов для проверки подсветки синтаксиса. Каждый фрагмент содержит типичные элементы языка (ключевые слова, строки, числа, комментарии, скобки), чтобы проверить работу парсера.

### CSS
```css
.container {
  display: flex;
  justify-content: center;
  align-items: center;
  background-color: #f0f0f0;
  padding: 20px;
  transition: all 0.3s ease-in-out;
}

.button:hover {
  transform: scale(1.1);
  color: var(--primary-color, #333);
  border: 1px solid rgba(0, 0, 0, 0.1);
}

#main-header {
  font-size: 2rem;
  margin-bottom: 1rem;
}
```

### GLSL
```glsl
precision mediump float;

uniform float u_time;
uniform vec2 u_resolution;
varying vec2 v_texCoord;

void main() {
    vec2 uv = gl_FragCoord.xy / u_resolution.xy;
    float dist = distance(uv, vec2(0.5));
    
    vec3 color = 0.5 + 0.5 * cos(u_time + uv.xyx + vec3(0, 2, 4));
    
    if (dist < 0.5) {
        gl_FragColor = vec4(color, 1.0);
    } else {
        gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);
    }
}
```

### HTML
```html
<section class="wrapper" id="main-section">
  <h1 class="title">Hello Syntax Highlighting</h1>
  <p>This is a <strong>test</strong> fragment for HTML.</p>
  
  <ul class="list-container">
    <li class="item active">Item One</li>
    <li class="item">Item Two</li>
  </ul>

  <form action="/submit" method="POST">
    <input type="text" name="username" placeholder="Enter name..." required />
    <button type="submit" disabled>Submit</button>
  </form>
</section>
```

### JS
```javascript
const fetchData = async (url) => {
  try {
    const response = await fetch(url);
    if (!response.ok) {
      throw new Error(`HTTP error! status: ${response.status}`);
    }
    const data = await response.json();
    
    return data.map(item => ({
      id: item.id,
      name: item.name.toUpperCase(),
      isActive: true
    }));
  } catch (error) {
    console.error(`Failed to fetch: ${error.message}`);
    return [];
  }
};

const user = { name: "Alice", age: 30, roles: ['admin', 'editor'] };
console.log(`User loaded: ${user.name}`);
```

### JSON
```json
{
  "id": 1024,
  "status": "success",
  "data": {
    "items": [
      {
        "uuid": "550e8400-e29b-41d4-a716-446655440000",
        "label": "Test Object",
        "value": 42.5,
        "is_valid": true
      },
      {
        "uuid": "678e8400-e29b-41d4-a716-446655441111",
        "label": "Null Object",
        "value": null,
        "is_valid": false
      }
    ],
    "metadata": {
      "version": "1.0.2",
      "tags": ["dev", "test", "syntax"]
    }
  }
}
```

### Rust
```rust
struct Player {
    name: String,
    health: i32,
    is_alive: bool,
}

impl Player {
    fn new(name: &str) -> Self {
        Self {
            name: name.to_string(),
            health: 100,
            is_alive: true,
        }
    }

    fn take_damage(&mut self, amount: i32) {
        self.health -= amount;
        if self.health <= 0 {
            self.is_alive = false;
            println!("{} has fallen!", self.name);
        }
    }
}

fn main() {
    let mut player = Player::new("Aragorn");
    player.take_damage(30);
    
    match player.is_alive {
        true => println!("{} is still fighting.", player.name),
        false => println!("Game Over."),
    }
}
```

### Zig
```zig
const std = @import("std");

const User = struct {
    id: u32,
    name: []const u8,
    active: bool,
};

pub fn main() !void {
    const allocator = std.heap.page_allocator;
    
    var user = User{
        .id = 1,
        .name = "Ziggy",
        .active = true,
    };

    std.debug.print("User: {s} (ID: {d})\n", .{user.name, user.id});

    var i: u32 = 0;
    while (i < 5) : (i += 1) {
        if (i == 3) {
            std.debug.print("Reached three!\n", .{});
        }
        std.debug.print("Count: {}\n", .{i});
    }
}
```

### XML
```xml
<?xml version="1.0" encoding="UTF-8"?>
<catalog xmlns:ext="http://example.com/ext">
    <!-- Main product list -->
    <product id="P-001" category="electronics">
        <name>Smartphone X</name>
        <price currency="USD">699.99</price>
        <description>
            A high-end smartphone with <ext:feature>OLED</ext:feature> display.
        </description>
        <stock>
            <item status="in_stock">45</item>
            <item status="low_stock">5</item>
        </stock>
    </product>
    <product id="P-002" category="books">
        <name>Learning XML</name>
        <price currency="EUR">29.50</price>
        <description>A comprehensive guide to XML structures.</description>
        <stock>
            <item status="out_of_stock">0</item>
        </stock>
    </product>
</catalog>
```

### DTD
```dtd
<!ELEMENT note (to, from, heading, body)>
<!ELEMENT to (#PCDATA)>
<!ELEMENT from (#PCDATA)>
<!ELEMENT heading (#PCDATA)>
<!ELEMENT body (#PCDATA)>

<!ATTLIST note
    id ID #REQUIRED
    priority (high|medium|low) "medium">

<!ELEMENT body (#PCDATA)>
<!ENTITY copyright "Copyright 2023 Test Corp">
<!ENTITY footer "Sent via Automated System">
```
### CPP
```cpp
std::string json_query_text()
{
    return embedded_query(highlight_json_query, highlight_json_query_len);
}

std::string rust_query_text()
{
    return embedded_query(highlight_rust_query, highlight_rust_query_len);
}

std::string zig_query_text()
{
    return embedded_query(highlight_zig_query, highlight_zig_query_len);
}

std::string xml_query_text()
{
    return embedded_query(highlight_xml_query, highlight_xml_query_len);
}

std::string dtd_query_text()
{
    return embedded_query(highlight_dtd_query, highlight_dtd_query_len);
}

struct Capture
{
    std::uint32_t start = 0;
    std::uint32_t end = 0;
    std::string type;
    std::uint32_t pattern_index = 0;
};

```
