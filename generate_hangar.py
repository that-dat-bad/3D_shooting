import os
import math

class ObjBuilder:
    def __init__(self):
        self.vertices = []
        self.normals = []
        self.texcoords = []
        self.materials = {} # mat_name -> list of faces
        self.current_material = "default"

    def set_material(self, name):
        self.current_material = name
        if name not in self.materials:
            self.materials[name] = []

    def add_vertex(self, x, y, z):
        self.vertices.append((x, y, z))
        return len(self.vertices)

    def add_normal(self, nx, ny, nz):
        l = math.sqrt(nx*nx + ny*ny + nz*nz)
        if l > 0:
            nx, ny, nz = nx/l, ny/l, nz/l
        self.normals.append((nx, ny, nz))
        return len(self.normals)

    def add_texcoord(self, u, v):
        self.texcoords.append((u, v))
        return len(self.texcoords)

    def add_quad(self, p1, p2, p3, p4, normal=None, uvs=None):
        """p1, p2, p3, p4: (x,y,z) counter-clockwise from front"""
        if normal is None:
            v1 = (p2[0]-p1[0], p2[1]-p1[1], p2[2]-p1[2])
            v2 = (p3[0]-p1[0], p3[1]-p1[1], p3[2]-p1[2])
            nx = v1[1]*v2[2] - v1[2]*v2[1]
            ny = v1[2]*v2[0] - v1[0]*v2[2]
            nz = v1[0]*v2[1] - v1[1]*v2[0]
            normal = (nx, ny, nz)
        
        nid = self.add_normal(*normal)
        
        if uvs is None:
            uvs = [(0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0)]
            
        t1 = self.add_texcoord(*uvs[0])
        t2 = self.add_texcoord(*uvs[1])
        t3 = self.add_texcoord(*uvs[2])
        t4 = self.add_texcoord(*uvs[3])

        v1 = self.add_vertex(*p1)
        v2 = self.add_vertex(*p2)
        v3 = self.add_vertex(*p3)
        v4 = self.add_vertex(*p4)

        if self.current_material not in self.materials:
            self.materials[self.current_material] = []

        # 2 triangles
        self.materials[self.current_material].append([(v1, t1, nid), (v2, t2, nid), (v3, t3, nid)])
        self.materials[self.current_material].append([(v1, t1, nid), (v3, t3, nid), (v4, t4, nid)])

    def add_box(self, x_min, y_min, z_min, x_max, y_max, z_max, mat=None):
        if mat: self.set_material(mat)
        
        # Bottom (-Y)
        self.add_quad(
            (x_min, y_min, z_max), (x_max, y_min, z_max), (x_max, y_min, z_min), (x_min, y_min, z_min),
            (0, -1, 0)
        )
        # Top (+Y)
        self.add_quad(
            (x_min, y_max, z_min), (x_max, y_max, z_min), (x_max, y_max, z_max), (x_min, y_max, z_max),
            (0, 1, 0)
        )
        # Front (+Z)
        self.add_quad(
            (x_min, y_min, z_max), (x_max, y_min, z_max), (x_max, y_max, z_max), (x_min, y_max, z_max),
            (0, 0, 1)
        )
        # Back (-Z)
        self.add_quad(
            (x_max, y_min, z_min), (x_min, y_min, z_min), (x_min, y_max, z_min), (x_max, y_max, z_min),
            (0, 0, -1)
        )
        # Left (-X)
        self.add_quad(
            (x_min, y_min, z_min), (x_min, y_min, z_max), (x_min, y_max, z_max), (x_min, y_max, z_min),
            (-1, 0, 0)
        )
        # Right (+X)
        self.add_quad(
            (x_max, y_min, z_max), (x_max, y_min, z_min), (x_max, y_max, z_min), (x_max, y_max, z_max),
            (1, 0, 0)
        )

    def add_beam_between_points(self, p1, p2, width, height, mat=None):
        """Creates a box beam connecting p1 to p2"""
        if mat: self.set_material(mat)
        
        dx = p2[0] - p1[0]
        dy = p2[1] - p1[1]
        dz = p2[2] - p1[2]
        length = math.sqrt(dx*dx + dy*dy + dz*dz)
        if length < 0.0001: return

        # Forward direction
        fx, fy, fz = dx/length, dy/length, dz/length
        
        # Up reference
        up = (0.0, 1.0, 0.0) if abs(fy) < 0.9 else (1.0, 0.0, 0.0)
        
        # Right vector = Forward x Up
        rx = fy*up[2] - fz*up[1]
        ry = fz*up[0] - fx*up[2]
        rz = fx*up[1] - fy*up[0]
        rl = math.sqrt(rx*rx + ry*ry + rz*rz)
        rx, ry, rz = rx/rl, ry/rl, rz/rl
        
        # True Up = Right x Forward
        ux = ry*fz - rz*fy
        uy = rz*fx - rx*fz
        uz = rx*fy - ry*fx
        
        hw = width * 0.5
        hh = height * 0.5
        
        # 4 corners at p1
        c1 = (p1[0] - rx*hw - ux*hh, p1[1] - ry*hw - uy*hh, p1[2] - rz*hw - uz*hh)
        c2 = (p1[0] + rx*hw - ux*hh, p1[1] + ry*hw - uy*hh, p1[2] + rz*hw - uz*hh)
        c3 = (p1[0] + rx*hw + ux*hh, p1[1] + ry*hw + uy*hh, p1[2] + rz*hw + uz*hh)
        c4 = (p1[0] - rx*hw + ux*hh, p1[1] - ry*hw + uy*hh, p1[2] - rz*hw + uz*hh)
        
        # 4 corners at p2
        d1 = (p2[0] - rx*hw - ux*hh, p2[1] - ry*hw - uy*hh, p2[2] - rz*hw - uz*hh)
        d2 = (p2[0] + rx*hw - ux*hh, p2[1] + ry*hw - uy*hh, p2[2] + rz*hw - uz*hh)
        d3 = (p2[0] + rx*hw + ux*hh, p2[1] + ry*hw + uy*hh, p2[2] + rz*hw + uz*hh)
        d4 = (p2[0] - rx*hw + ux*hh, p2[1] - ry*hw + uy*hh, p2[2] - rz*hw + uz*hh)
        
        # 4 side quads
        self.add_quad(c1, c2, d2, d1)
        self.add_quad(c2, c3, d3, d2)
        self.add_quad(c3, c4, d4, d3)
        self.add_quad(c4, c1, d1, d4)
        
        # End caps
        self.add_quad(c4, c3, c2, c1)
        self.add_quad(d1, d2, d3, d4)

    def export_obj(self, filepath, mtl_filename):
        with open(filepath, "w", encoding="utf-8") as f:
            f.write(f"# Procedural Military/Industrial Aircraft Hangar\n")
            f.write(f"mtllib {mtl_filename}\n\n")
            
            for v in self.vertices:
                f.write(f"v {v[0]:.6f} {v[1]:.6f} {v[2]:.6f}\n")
            f.write("\n")
            
            for vt in self.texcoords:
                f.write(f"vt {vt[0]:.6f} {vt[1]:.6f}\n")
            f.write("\n")
            
            for vn in self.normals:
                f.write(f"vn {vn[0]:.6f} {vn[1]:.6f} {vn[2]:.6f}\n")
            f.write("\n")
            
            for mat, faces in self.materials.items():
                if not faces: continue
                f.write(f"usemtl {mat}\n")
                f.write("s 1\n")
                for tri in faces:
                    f.write(f"f {tri[0][0]}/{tri[0][1]}/{tri[0][2]} {tri[1][0]}/{tri[1][1]}/{tri[1][2]} {tri[2][0]}/{tri[2][1]}/{tri[2][2]}\n")
                f.write("\n")

def generate_hangar():
    builder = ObjBuilder()

    X_HALF = 18.0
    Z_HALF = 24.0
    Y_WALL = 8.8      # 側壁トップの高さ
    Y_PEAK = 12.8     # 屋根頂部の高さ

    # 1. 床面 (Concrete Floor)
    builder.add_box(-X_HALF - 0.5, -0.2, -Z_HALF - 0.5, X_HALF + 0.5, 0.0, Z_HALF + 0.5, "Hangar_Floor")

    # タキシング・誘導黄色ライン
    builder.add_box(-0.35, 0.003, -Z_HALF, 0.35, 0.006, Z_HALF, "Hangar_Marking")
    # 駐機停止T字ライン
    builder.add_box(-4.5, 0.003, 5.0, 4.5, 0.006, 5.7, "Hangar_Marking")
    builder.add_box(-0.35, 0.003, 5.7, 0.35, 0.006, 7.5, "Hangar_Marking")
    # 駐機枠（パーキングベイ矩形）
    bay_x = 7.5
    bay_z1 = -8.0
    bay_z2 = 8.0
    line_w = 0.25
    builder.add_box(-bay_x, 0.003, bay_z1, bay_x, 0.006, bay_z1 + line_w, "Hangar_Marking")
    builder.add_box(-bay_x, 0.003, bay_z2 - line_w, bay_x, 0.006, bay_z2, "Hangar_Marking")
    builder.add_box(-bay_x, 0.003, bay_z1, -bay_x + line_w, 0.006, bay_z2, "Hangar_Marking")
    builder.add_box(bay_x - line_w, 0.003, bay_z1, bay_x, 0.006, bay_z2, "Hangar_Marking")

    # 2. 壁面 (Walls)
    # 側壁 基礎 (コンクリート腰壁)
    builder.add_box(-X_HALF - 0.5, 0.0, -Z_HALF, -X_HALF, 2.2, Z_HALF, "Hangar_Wall_Lower")
    builder.add_box(X_HALF, 0.0, -Z_HALF, X_HALF + 0.5, 2.2, Z_HALF, "Hangar_Wall_Lower")
    # 側壁 上部 (断熱波板スレート)
    builder.add_box(-X_HALF - 0.3, 2.2, -Z_HALF, -X_HALF, Y_WALL, Z_HALF, "Hangar_Wall_Upper")
    builder.add_box(X_HALF, 2.2, -Z_HALF, X_HALF + 0.3, Y_WALL, Z_HALF, "Hangar_Wall_Upper")

    # 奥壁 (Back Wall, Z = -Z_HALF)
    door_w = 11.5
    door_h = 7.5
    builder.add_box(-X_HALF, 0.0, -Z_HALF - 0.4, -door_w, Y_WALL, -Z_HALF, "Hangar_Wall_Upper")
    builder.add_box(door_w, 0.0, -Z_HALF - 0.4, X_HALF, Y_WALL, -Z_HALF, "Hangar_Wall_Upper")
    builder.add_box(-door_w, door_h, -Z_HALF - 0.4, door_w, Y_WALL, -Z_HALF, "Hangar_Wall_Upper")
    
    # 奥の妻壁 (Gable Wall, 厚みを持たせて両面対応)
    # 内側
    builder.add_quad(
        (-X_HALF, Y_WALL, -Z_HALF), (X_HALF, Y_WALL, -Z_HALF), (0.0, Y_PEAK, -Z_HALF), (-X_HALF, Y_WALL, -Z_HALF),
        (0, 0, 1)
    )
    # 外側
    builder.add_quad(
        (X_HALF, Y_WALL, -Z_HALF - 0.4), (-X_HALF, Y_WALL, -Z_HALF - 0.4), (0.0, Y_PEAK, -Z_HALF - 0.4), (X_HALF, Y_WALL, -Z_HALF - 0.4),
        (0, 0, -1)
    )

    # 格納庫シャッター (少し開いて隙間がある)
    shutter_open_gap = 1.6
    builder.add_box(-door_w + 0.2, shutter_open_gap, -Z_HALF - 0.15, door_w - 0.2, door_h + 0.2, -Z_HALF + 0.15, "Hangar_Shutter")
    for sy in [2.5, 3.5, 4.5, 5.5, 6.5]:
        builder.add_box(-door_w + 0.1, sy, -Z_HALF + 0.15, door_w - 0.1, sy + 0.15, -Z_HALF + 0.22, "Hangar_Steel_Accent")

    # 手前壁 (Front Wall, Z = +Z_HALF)
    builder.add_box(-X_HALF, 0.0, Z_HALF, -door_w, Y_WALL, Z_HALF + 0.4, "Hangar_Wall_Upper")
    builder.add_box(door_w, 0.0, Z_HALF, X_HALF, Y_WALL, Z_HALF + 0.4, "Hangar_Wall_Upper")
    builder.add_box(-door_w, door_h, Z_HALF, door_w, Y_WALL, Z_HALF + 0.4, "Hangar_Wall_Upper")
    # 手前妻壁
    # 内側
    builder.add_quad(
        (X_HALF, Y_WALL, Z_HALF), (-X_HALF, Y_WALL, Z_HALF), (0.0, Y_PEAK, Z_HALF), (X_HALF, Y_WALL, Z_HALF),
        (0, 0, -1)
    )
    # 外側
    builder.add_quad(
        (-X_HALF, Y_WALL, Z_HALF + 0.4), (X_HALF, Y_WALL, Z_HALF + 0.4), (0.0, Y_PEAK, Z_HALF + 0.4), (-X_HALF, Y_WALL, Z_HALF + 0.4),
        (0, 0, 1)
    )

    # 3. 屋根 (Roof Panels - 厚みのあるスレート屋根、外側・内側両面)
    roof_thickness = 0.25
    builder.set_material("Hangar_Roof")
    
    # 屋根 外面（上向き）
    # 左屋根外側
    builder.add_quad(
        (-X_HALF - 0.5, Y_WALL + roof_thickness, -Z_HALF - 0.5),
        (0.0, Y_PEAK + roof_thickness, -Z_HALF - 0.5),
        (0.0, Y_PEAK + roof_thickness, Z_HALF + 0.5),
        (-X_HALF - 0.5, Y_WALL + roof_thickness, Z_HALF + 0.5),
        (- (Y_PEAK - Y_WALL), X_HALF, 0)
    )
    # 右屋根外側
    builder.add_quad(
        (0.0, Y_PEAK + roof_thickness, -Z_HALF - 0.5),
        (X_HALF + 0.5, Y_WALL + roof_thickness, -Z_HALF - 0.5),
        (X_HALF + 0.5, Y_WALL + roof_thickness, Z_HALF + 0.5),
        (0.0, Y_PEAK + roof_thickness, Z_HALF + 0.5),
        ((Y_PEAK - Y_WALL), X_HALF, 0)
    )
    
    # 屋根 内面（天井・下向き）
    # 左屋根内側
    builder.add_quad(
        (-X_HALF, Y_WALL, Z_HALF),
        (0.0, Y_PEAK, Z_HALF),
        (0.0, Y_PEAK, -Z_HALF),
        (-X_HALF, Y_WALL, -Z_HALF),
        ((Y_PEAK - Y_WALL), -X_HALF, 0)
    )
    # 右屋根内側
    builder.add_quad(
        (0.0, Y_PEAK, Z_HALF),
        (X_HALF, Y_WALL, Z_HALF),
        (X_HALF, Y_WALL, -Z_HALF),
        (0.0, Y_PEAK, -Z_HALF),
        (- (Y_PEAK - Y_WALL), -X_HALF, 0)
    )

    # 4. 【本命】鉄骨トラス大梁（Main Truss Beams）
    # 屋根面より確実に「下」に収まるよう厳密な高さ計算を行う！
    z_trusses = [-18.0, -12.0, -6.0, 0.0, 6.0, 12.0, 18.0]
    
    beam_w = 0.35  # 大梁断面
    beam_h = 0.35
    chord_y = 7.8  # 下弦材の高さ (水平ビーム中心)
    
    # 屋根内面からトラス上弦材中心までのクリアランス（0.7m 下げることで絶対に飛び出さない）
    TRUSS_TOP_CLEARANCE = 0.70
    
    def get_truss_top_y(x_val):
        """任意の x におけるトラス上弦材の中心線 Y 座標 (中央が高く、端が低い！)"""
        ratio = abs(x_val) / (X_HALF - 0.8)
        ratio = min(1.0, max(0.0, ratio))
        y_center = Y_PEAK - TRUSS_TOP_CLEARANCE          # 中央 (x=0) での高さ: 12.8 - 0.7 = 12.1m
        y_end = Y_WALL - TRUSS_TOP_CLEARANCE             # 端 (x=17.2) での高さ: 8.8 - 0.7 = 8.1m
        return y_center - (y_center - y_end) * ratio

    for z in z_trusses:
        # A. 左右の支柱（H鋼メインカラム: Y=0 から Y_WALL まで）
        builder.add_box(-X_HALF + 0.1, 0.0, z - 0.35, -X_HALF + 0.8, Y_WALL - 0.2, z + 0.35, "Hangar_Steel_Beam")
        builder.add_box(X_HALF - 0.8, 0.0, z - 0.35, X_HALF - 0.1, Y_WALL - 0.2, z + 0.35, "Hangar_Steel_Beam")
        # 柱のベースプレート (黄色警告)
        builder.add_box(-X_HALF, 0.0, z - 0.45, -X_HALF + 0.9, 0.3, z + 0.45, "Hangar_Steel_Accent")
        builder.add_box(X_HALF - 0.9, 0.0, z - 0.45, X_HALF, 0.3, z + 0.45, "Hangar_Steel_Accent")

        x_end = X_HALF - 0.8  # 17.2m
        y_top_end = get_truss_top_y(x_end)     # 8.1m
        y_top_center = get_truss_top_y(0.0)    # 12.1m

        # B. 大梁・下弦材 (Bottom Chord: 水平ビーム Y = 7.8m)
        p_left_bottom = (-x_end, chord_y, z)
        p_right_bottom = (x_end, chord_y, z)
        builder.add_beam_between_points(p_left_bottom, p_right_bottom, beam_w, beam_h, "Hangar_Steel_Beam")

        # C. 大梁・上弦材 (Top Chord: 屋根内面より確実に低い傾斜ビーム)
        p_left_top = (-x_end, y_top_end, z)
        p_center_top = (0.0, y_top_center, z)
        p_right_top = (x_end, y_top_end, z)
        builder.add_beam_between_points(p_left_top, p_center_top, beam_w, beam_h, "Hangar_Steel_Beam")
        builder.add_beam_between_points(p_center_top, p_right_top, beam_w, beam_h, "Hangar_Steel_Beam")

        # D. トラス内部の垂直材・斜材 (Truss Web Members)
        # 中央 (x=0) から 端 (x=x_end) に向かって正しくノードを並べる！
        divisions = 7
        for side in [-1.0, 1.0]:
            # 各ノードの位置: i=0 が中央(x=0)、i=divisions が端(x=x_end)
            for i in range(divisions):
                x_curr = side * (x_end * (i / float(divisions)))
                x_next = side * (x_end * ((i + 1) / float(divisions)))
                
                # 下弦材接合面 (ビーム上面)
                b_curr = (x_curr, chord_y + beam_h * 0.5, z)
                b_next = (x_next, chord_y + beam_h * 0.5, z)
                
                # 上弦材接合面 (ビーム下面)
                t_curr = (x_curr, get_truss_top_y(x_curr) - beam_h * 0.5, z)
                t_next = (x_next, get_truss_top_y(x_next) - beam_h * 0.5, z)
                
                # 垂直材 (中央以外のノードで直下へ降ろす)
                if i > 0:
                    builder.add_beam_between_points(b_curr, t_curr, 0.20, 0.20, "Hangar_Truss")
                
                # 斜材 (Warren Truss: ジグザグに結ぶ)
                if i % 2 == 0:
                    # (下・現) -> (上・次)
                    builder.add_beam_between_points(b_curr, t_next, 0.18, 0.18, "Hangar_Truss")
                else:
                    # (上・現) -> (下・次)
                    builder.add_beam_between_points(t_curr, b_next, 0.18, 0.18, "Hangar_Truss")

            # 端部の支柱との縦接合材
            builder.add_beam_between_points(
                (side * x_end, chord_y + beam_h * 0.5, z),
                (side * x_end, y_top_end - beam_h * 0.5, z),
                0.22, 0.22, "Hangar_Truss"
            )

        # E. 方杖ブレース (Knee Braces: 柱と下弦材の斜め補強材)
        builder.add_beam_between_points((-x_end, 5.5, z), (-x_end + 3.0, chord_y, z), 0.22, 0.22, "Hangar_Steel_Accent")
        builder.add_beam_between_points((x_end, 5.5, z), (x_end - 3.0, chord_y, z), 0.22, 0.22, "Hangar_Steel_Accent")

        # F. ハイベイ照明（吊り下げインダストリアルライト）
        for lx in [-8.5, 8.5]:
            lamp_y = chord_y - 0.3
            builder.add_beam_between_points((lx, chord_y, z), (lx, lamp_y, z), 0.06, 0.06, "Hangar_Steel_Beam")
            builder.add_box(lx - 0.4, lamp_y - 0.25, z - 0.4, lx + 0.4, lamp_y, z + 0.4, "Hangar_Lamp_Body")
            builder.add_box(lx - 0.32, lamp_y - 0.28, z - 0.32, lx + 0.32, lamp_y - 0.24, z + 0.32, "Hangar_Lamp_Bulb")

    # 5. 【小梁・繋ぎ梁】（Longitudinal Girders / Purling）
    # 大トラス間を長手方向（Z軸）に繋ぐ鉄骨ビーム
    for gx in [-17.2, -11.0, -5.0, 5.0, 11.0, 17.2]:
        builder.add_beam_between_points((gx, chord_y, -Z_HALF + 1.0), (gx, chord_y, Z_HALF - 1.0), 0.22, 0.22, "Hangar_Steel_Beam")
    
    # 棟木（屋根頂部真下のキールビーム: 上弦材中央直下）
    builder.add_beam_between_points((0.0, get_truss_top_y(0.0) - beam_h * 0.5, -Z_HALF + 1.0), (0.0, get_truss_top_y(0.0) - beam_h * 0.5, Z_HALF - 1.0), 0.25, 0.25, "Hangar_Steel_Beam")
    
    # 母屋（屋根傾斜面に沿った縦繋ぎ梁: 上弦材と屋根の間に完全に収まる）
    for px_ratio in [0.35, 0.70]:
        for side in [-1.0, 1.0]:
            px = side * (X_HALF - 0.8) * px_ratio
            py = get_truss_top_y(px)
            builder.add_beam_between_points((px, py, -Z_HALF + 1.0), (px, py, Z_HALF - 1.0), 0.18, 0.18, "Hangar_Steel_Beam")

    # 6. キャットウォーク（中央点検通路 & 手すり）
    cw_y = chord_y - 0.45
    cw_w = 0.85
    # 通路床
    builder.add_box(-cw_w, cw_y, -21.0, cw_w, cw_y + 0.08, 21.0, "Hangar_Steel_Beam")
    # 手すり左右
    for side in [-1.0, 1.0]:
        rx = side * cw_w
        builder.add_beam_between_points((rx, cw_y + 0.9, -21.0), (rx, cw_y + 0.9, 21.0), 0.05, 0.05, "Hangar_Steel_Accent")
        builder.add_beam_between_points((rx, cw_y + 0.45, -21.0), (rx, cw_y + 0.45, 21.0), 0.04, 0.04, "Hangar_Steel_Accent")
        for cz in range(-20, 21, 3):
            builder.add_beam_between_points((rx, cw_y, cz), (rx, cw_y + 0.9, cz), 0.05, 0.05, "Hangar_Steel_Accent")

    # 7. 壁面の工業用配管・パイプ群 (Wall Utility Pipes)
    for p_side in [-1.0, 1.0]:
        px_pipe = p_side * (X_HALF - 0.4)
        builder.add_beam_between_points((px_pipe, 3.2, -Z_HALF + 1.0), (px_pipe, 3.2, Z_HALF - 1.0), 0.14, 0.14, "Hangar_Pipe_Red")
        builder.add_beam_between_points((px_pipe, 3.6, -Z_HALF + 1.0), (px_pipe, 3.6, Z_HALF - 1.0), 0.10, 0.10, "Hangar_Steel_Accent")
        builder.add_beam_between_points((px_pipe, 4.3, -Z_HALF + 1.0), (px_pipe, 4.3, Z_HALF - 1.0), 0.30, 0.22, "Hangar_Steel_Beam")

    out_obj = "Resources/models/title/hangar.obj"
    out_mtl = "hangar.mtl"
    builder.export_obj(out_obj, out_mtl)
    print(f"Generated {out_obj} with {len(builder.vertices)} vertices!")

if __name__ == "__main__":
    generate_hangar()
