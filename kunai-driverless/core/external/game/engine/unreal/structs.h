#pragma once 

namespace game {
    struct f_vector {
        double x;
        double y;
        double z;

        f_vector operator + ( const f_vector& other ) const { return { this->x + other.x, this->y + other.y, this->z + other.z }; }
        f_vector operator - ( const f_vector& other ) const { return { this->x - other.x, this->y - other.y, this->z - other.z }; }
        f_vector operator * ( const f_vector& other ) const { return { this->x * other.x, this->y * other.y, this->z * other.z }; }
        f_vector operator * ( double offset ) const { return { this->x * offset, this->y * offset, this->z * offset }; }
        f_vector operator / ( double offset ) const { return { this->x / offset, this->y / offset, this->z / offset }; }

        f_vector& operator *= ( const double other ) { this->x *= other; this->y *= other; this->z *= other; return *this; }
        f_vector& operator /= ( const double other ) { this->x /= other; this->y /= other; this->z /= other; return *this; }

        f_vector& operator = ( const f_vector& other ) { this->x = other.x; this->y = other.y; this->z = other.z; return *this; }
        f_vector& operator += ( const f_vector& other ) { this->x += other.x; this->y += other.y; this->z += other.z; return *this; }
        f_vector& operator -= ( const f_vector& other ) { this->x -= other.x; this->y -= other.y; this->z -= other.z; return *this; }
        f_vector& operator *= ( const f_vector& other ) { this->x *= other.x; this->y *= other.y; this->z *= other.z; return *this; }
        f_vector& operator /= ( const f_vector& other ) { this->x /= other.x; this->y /= other.y; this->z /= other.z; return *this; }

        operator bool( ) { return bool( this->x || this->y || this->z ); }
        friend bool operator == ( const f_vector& a, const f_vector& b ) { return a.x == b.x && a.y == b.y && a.z == b.z; }
        friend bool operator != ( const f_vector& a, const f_vector& b ) { return !( a == b ); }

        double vector_scalar( const f_vector& v ) const { return x * v.x + y * v.y + z * v.z; }
        double distance_to( f_vector v ) const { return sqrt( pow( v.x - x, 2.0 ) + pow( v.y - y, 2.0 ) + pow( v.z - z, 2.0 ) ) / 100.0; }
        double distance( f_vector v ) const { return sqrt( vector_scalar( v ) ); }
        bool is_valid( ) const { return this->x && this->y && this->z; }
        void in_validate( ) { this->x = this->y = this->z = 0; }
        double length( ) const { return sqrt( x * x + y * y + z * z ); }

        float vector_distance( const f_vector& a, const f_vector& b ) {
            float dx = a.x - b.x;
            float dy = a.y - b.y;
            float dz = a.z - b.z;
            return std::sqrt( dx * dx + dy * dy + dz * dz );
        }

        f_vector normalized( ) const {
            auto len = sqrt( x * x + y * y + z * z );
            if ( len < 1e-8 ) {
                return { 0.0, 0.0, 0.0 };
            }

            return { x / len, y / len, z / len };
        }

        float size_squared( ) const {
            return x * x + y * y + z * z;
        }

        f_vector cross( const f_vector& other ) const {
            return {
                this->y * other.z - this->z * other.y,
                this->z * other.x - this->x * other.z,
                this->x * other.y - this->y * other.x
            };
        }

        f_vector lerp( const f_vector& other, float alpha ) const {
            return *this * ( 1.0f - alpha ) + other * alpha;
        }

        bool is_zero( double tolerance = 1e-6 ) const {
            return fabs( x ) < tolerance && fabs( y ) < tolerance && fabs( z ) < tolerance;
        }
    };

    struct f_vector2d {
        f_vector2d( ) : x( ), y( ) { }
        f_vector2d( int x, int y ) : x( x ), y( y ) { }
        f_vector2d( double x, double y ) : x( x ), y( y ) { }

        f_vector2d operator + ( const f_vector2d& other ) const { return { this->x + other.x, this->y + other.y }; }
        f_vector2d operator - ( const f_vector2d& other ) const { return { this->x - other.x, this->y - other.y }; }
        f_vector2d operator * ( double offset ) const { return { this->x * offset, this->y * offset }; }
        f_vector2d operator / ( double offset ) const { return { this->x / offset, this->y / offset }; }

        f_vector2d& operator *= ( const double other ) { this->x *= other; this->y *= other; return *this; }
        f_vector2d& operator /= ( const double other ) { this->x /= other; this->y /= other; return *this; }

        f_vector2d& operator = ( const f_vector2d& other ) { this->x = other.x; this->y = other.y; return *this; }
        f_vector2d& operator += ( const f_vector2d& other ) { this->x += other.x; this->y += other.y; return *this; }
        f_vector2d& operator -= ( const f_vector2d& other ) { this->x -= other.x; this->y -= other.y; return *this; }
        f_vector2d& operator *= ( const f_vector2d& other ) { this->x *= other.x; this->y *= other.y; return *this; }
        f_vector2d& operator /= ( const f_vector2d& other ) { this->x /= other.x; this->y /= other.y; return *this; }

        operator bool( ) { return bool( this->x || this->y ); }
        friend bool operator == ( const f_vector2d& a, const f_vector2d& b ) { return a.x == b.x && a.y == b.y; }
        friend bool operator != ( const f_vector2d& a, const f_vector2d& b ) { return !( a == b ); }

        bool in_rect( double radius ) const;
        bool in_circle( double radius ) const;
        bool in_screen( ) const;
        double vector_scalar( const f_vector2d& v ) { return x * v.x + y * v.y; }
        double distance_to( f_vector2d v ) { return double( sqrtf( powf( v.x - this->x, 2.0 ) + powf( v.y - this->y, 2.0 ) ) ); }
        double distance( ) { return double( sqrt( x * x + y * y ) ); }
        bool is_valid( ) const { return this->x && this->y; }

        double x, y;
    };

    struct f_quaternion {
        double x;
        double y;
        double z;
        double w;

        f_quaternion( ) : x( 0 ), y( 0 ), z( 0 ), w( 1 ) { }
        f_quaternion( double x, double y, double z, double w ) : x( x ), y( y ), z( z ), w( w ) { }

        f_vector multiply( const f_vector& point ) const {
            double num = x * 2.00;
            double num2 = y * 2.00;
            double num3 = z * 2.00;
            double num4 = x * num;
            double num5 = y * num2;
            double num6 = z * num3;
            double num7 = x * num2;
            double num8 = x * num3;
            double num9 = y * num3;
            double num10 = w * num;
            double num11 = w * num2;
            double num12 = w * num3;

            f_vector result;
            result.x = ( 1.00 - ( num5 + num6 ) ) * point.x +
                ( num7 - num12 ) * point.y + ( num8 + num11 ) * point.z;
            result.y = ( num7 + num12 ) * point.x +
                ( 1.00 - ( num4 + num6 ) ) * point.y +
                ( num9 - num10 ) * point.z;
            result.z = ( num8 - num11 ) * point.x + ( num9 + num10 ) * point.y +
                ( 1.00 - ( num4 + num5 ) ) * point.z;
            return result;
        }
    };

    struct f_rotator {
        double pitch;
        double yaw;
        double roll;

        f_rotator( )
            : pitch( 0 ), yaw( 0 ), roll( 0 ) {
        }

        f_rotator( double pitch, double yaw, double roll )
            : pitch( pitch ),
            yaw( yaw ),
            roll( roll ) {
        }

        f_rotator operator-( const f_rotator& v ) {
            return f_rotator( pitch - v.pitch, yaw - v.yaw, roll - v.roll );
        }

        f_rotator operator+( const f_rotator& v ) {
            return f_rotator( pitch + v.pitch, yaw + v.yaw, roll + v.roll );
        }

        f_rotator operator*( float scale ) const {
            return f_rotator( pitch * scale, yaw * scale, roll * scale );
        }

        f_rotator operator/( float scale ) const {
            const float r_scale = 1.f / scale;
            return f_rotator( pitch * r_scale, yaw * r_scale, roll * r_scale );
        }

        f_rotator operator+( float a ) const {
            return f_rotator( pitch + a, yaw + a, roll + a );
        }

        f_rotator operator-( float a ) const {
            return f_rotator( pitch - a, yaw - a, roll - a );
        }

        f_rotator operator*( const f_rotator& v ) const {
            return f_rotator( pitch * v.pitch, yaw * v.yaw, roll * v.roll );
        }

        f_rotator operator/( const f_rotator& v ) const {
            return f_rotator( pitch / v.pitch, yaw / v.yaw, roll / v.roll );
        }

        float operator|( const f_rotator& v ) const {
            return pitch * v.pitch + yaw * v.yaw + roll * v.roll;
        }

        float operator^( const f_rotator& v ) const {
            return pitch * v.yaw - yaw * v.pitch - roll * v.roll;
        }

        f_rotator& operator+=( const f_rotator& v ) {
            pitch += v.pitch;
            yaw += v.yaw;
            roll += v.roll;
            return *this;
        }

        f_rotator& operator-=( const f_rotator& v ) {
            pitch -= v.pitch;
            yaw -= v.yaw;
            roll -= v.roll;
            return *this;
        }

        f_rotator& operator*=( const f_rotator& v ) {
            pitch *= v.pitch;
            yaw *= v.yaw;
            roll *= v.roll;
            return *this;
        }

        f_rotator& operator/=( const f_rotator& v ) {
            pitch /= v.pitch;
            yaw /= v.yaw;
            roll /= v.roll;
            return *this;
        }

        bool operator==( const f_rotator& src ) const {
            return ( src.pitch == pitch ) && ( src.yaw == yaw ) && ( src.roll == roll );
        }

        bool operator!=( const f_rotator& src ) const {
            return ( src.pitch != pitch ) || ( src.yaw != yaw ) || ( src.roll != roll );
        }

        f_vector multiply( const f_vector& point ) const {
            double pitch_rad = pitch * ( std::numbers::pi / 180.0 );
            double yaw_rad = yaw * ( std::numbers::pi / 180.0 );
            double roll_rad = roll * ( std::numbers::pi / 180.0 );

            double sp = sin( pitch_rad );
            double cp = cos( pitch_rad );
            double sy = sin( yaw_rad );
            double cy = cos( yaw_rad );
            double sr = sin( roll_rad );
            double cr = cos( roll_rad );

            double m[ 3 ][ 3 ] = {
                { cy * cr + sy * sp * sr, cy * sr - sy * sp * cr, sy * cp },
                { -cp * sr, cp * cr, sp },
                { sy * cr - cy * sp * sr, sy * sr + cy * sp * cr, cy * cp }
            };

            f_vector result;
            result.x = point.x * m[ 0 ][ 0 ] + point.y * m[ 0 ][ 1 ] + point.z * m[ 0 ][ 2 ];
            result.y = point.x * m[ 1 ][ 0 ] + point.y * m[ 1 ][ 1 ] + point.z * m[ 1 ][ 2 ];
            result.z = point.x * m[ 2 ][ 0 ] + point.y * m[ 2 ][ 1 ] + point.z * m[ 2 ][ 2 ];

            return result;
        }

        f_vector get_forward_vector( ) {
            auto pitch_degree = static_cast< double >( this->pitch * ( std::numbers::pi / 180 ) );
            auto yaw_degree = static_cast< double >( this->yaw * ( std::numbers::pi / 180 ) );

            auto cp = cos( pitch_degree );
            auto sp = sin( pitch_degree );
            auto cy = cos( yaw_degree );
            auto sy = sin( yaw_degree );

            return f_vector(
                cp * cy,
                cp * sy,
                sp
            );
        }

        f_vector unrotate_vector( const f_vector& vec ) const {
            float pitch_rad = pitch * ( std::numbers::pi / 180.0f );
            float yaw_rad = yaw * ( std::numbers::pi / 180.0f );
            float roll_rad = roll * ( std::numbers::pi / 180.0f );

            float sp = sin( pitch_rad );
            float cp = cos( pitch_rad );
            float sy = sin( yaw_rad );
            float cy = cos( yaw_rad );
            float sr = sin( roll_rad );
            float cr = cos( roll_rad );

            float m[ 3 ][ 3 ] = {
                { cy * cr - sy * sp * sr, -cy * sr - sy * sp * cr, sy * cp },
                { sy * cr + cy * sp * sr, -sy * sr + cy * sp * cr, -cy * cp },
                { cp * sr, cp * cr, sp }
            };

            return f_vector(
                vec.x * m[ 0 ][ 0 ] + vec.y * m[ 0 ][ 1 ] + vec.z * m[ 0 ][ 2 ],
                vec.x * m[ 1 ][ 0 ] + vec.y * m[ 1 ][ 1 ] + vec.z * m[ 1 ][ 2 ],
                vec.x * m[ 2 ][ 0 ] + vec.y * m[ 2 ][ 1 ] + vec.z * m[ 2 ][ 2 ]
            );
        }

        double f_mod( double a1, double a2 ) {
            if ( a2 > 0.00000001 || a2 < -0.00000001 ) {
                double result = a1 - ( int )( a1 / a2 ) * a2;
                return result;
            }
            else {
                return 0.0;
            }
        }

        double clamp( double angle ) {
            angle = f_mod( angle, ( double )360.f );

            if ( angle < ( double )0.f ) {
                angle += ( double )360.f;
            }

            return angle;
        }

        double normalize( double angle ) {
            angle = clamp( angle );

            if ( angle > ( double )180.f ) {
                angle -= ( double )360.f;
            }

            return angle;
        }

        double clamp( double value, double min, double max ) {
            if ( value < min ) {
                return min;
            }
            else if ( value > max ) {
                return max;
            }
            else {
                return value;
            }
        }

        void normalize( ) {
            pitch = normalize( pitch );
            yaw = normalize( yaw );
            roll = normalize( roll );
        }

        bool is_valid( ) const { return this->pitch && this->yaw; }

        f_quaternion to_quaternion( ) const {
            double pitch_rad = pitch * ( std::numbers::pi / 180.0 );
            double yaw_rad = yaw * ( std::numbers::pi / 180.0 );
            double roll_rad = roll * ( std::numbers::pi / 180.0 );

            double sp = sin( pitch_rad * 0.5 );
            double cp = cos( pitch_rad * 0.5 );
            double sy = sin( yaw_rad * 0.5 );
            double cy = cos( yaw_rad * 0.5 );
            double sr = sin( roll_rad * 0.5 );
            double cr = cos( roll_rad * 0.5 );

            f_quaternion quat;
            quat.x = sr * cp * cy - cr * sp * sy;
            quat.y = cr * sp * cy + sr * cp * sy;
            quat.z = cr * cp * sy - sr * sp * cy;
            quat.w = cr * cp * cy + sr * sp * sy;

            return quat;
        }
    };

    struct f_linear_color {
        f_linear_color( ) :a( ), r( ), g( ), b( ) { }
        f_linear_color( int8_t a, int8_t r, int8_t g, int8_t b ) : a( a ), r( r ), g( g ), b( b ) { }

        int8_t a, r, g, b;
    };

    template< class type >
    class tarray {
    public:
        tarray( ) : data( ), count( ), max_count( ) { }
        tarray( type* data, std::uint32_t count, std::uint32_t max_count ) :
            data( data ), count( count ), max_count( max_count ) {
        }

        type get( std::uintptr_t idx ) {
            return g_process->read< type >(
                std::bit_cast< std::uintptr_t >( this->data ) + ( idx * sizeof( type ) )
            );
        }

        std::vector<type> get_itter( ) {
            if ( this->count > this->max_count )
                return {};

            std::vector<type> buffer( this->count );

            g_process->read_memory(
                std::bit_cast< std::uintptr_t >( this->data ),
                buffer.data( ),
                sizeof( type ) * this->count
            );

            return buffer;
        }

        std::uintptr_t get_addr( ) {
            return reinterpret_cast< std::uintptr_t >( this->data );
        }

        std::uint32_t size( ) const {
            return this->count;
        };

        std::uint32_t max_size( ) const {
            return this->max_count;
        };

        bool is_valid_index( std::int32_t index ) const {
            return index >= 0 && index < max_count && index > count;
        };

        bool is_valid( ) const {
            return utility::is_valid_pointer( this->data ) && count && max_count;
        };

        type* data;
        std::uint32_t count;
        std::uint32_t max_count;
    };

    template<typename type>
    struct t_set {
        tarray<type> sparse_array;
        uint8_t pad_10[ 0x40 ];
    };

    template<typename key, typename value>
    class t_pair {
    public:
        key first;
        value second;
    };

    template<typename type, typename value>
    struct t_map {
        t_set<t_pair<type, value>> pairs;
    };

    struct fbox_sphere_bounds final {
    public:
        f_vector orgin;
        f_vector box_extent;
        double sphere_radius;
    };

    struct f_plane : f_vector {

        f_plane( ) : w( 0 ) { }
        f_plane( double w ) : w( w ) { }

        f_vector to_vector( ) {
            f_vector value;
            value.x = this->x;
            value.y = this->y;
            value.z = this->z;
            return value;
        }

        double w;
    };

    class f_matrix {
    public:
        double m[ 4 ][ 4 ];
        f_plane x_plane, y_plane, z_plane, w_plane;

        f_matrix( ) : x_plane( ), y_plane( ), z_plane( ), w_plane( ) { }
        f_matrix( f_plane x_plane, f_plane y_plane, f_plane z_plane, f_plane w_plane )
            : x_plane( x_plane ), y_plane( y_plane ), z_plane( z_plane ), w_plane( w_plane ) {
        }
        f_matrix( f_rotator& rotator ) : x_plane( ), y_plane( ), z_plane( ), w_plane( ) {
            this->to_rotation_matrix( rotator );
        }

        f_matrix to_rotation_matrix( f_rotator& rotation ) {
            f_matrix matrix = {};

            auto rad_pitch = ( rotation.pitch * std::numbers::pi / 180.f );
            auto rad_yaw = ( rotation.yaw * std::numbers::pi / 180.f );
            auto rad_roll = ( rotation.roll * std::numbers::pi / 180.f );

            auto sin_pitch = sin( rad_pitch );
            auto cos_pitch = cos( rad_pitch );

            auto sin_yaw = sin( rad_yaw );
            auto cos_yaw = cos( rad_yaw );

            auto sin_roll = sin( rad_roll );
            auto cos_roll = cos( rad_roll );

            matrix.x_plane.x = cos_pitch * cos_yaw;
            matrix.x_plane.y = cos_pitch * sin_yaw;
            matrix.x_plane.z = sin_pitch;
            matrix.x_plane.w = 0.f;

            matrix.y_plane.x = sin_roll * sin_pitch * cos_yaw - cos_roll * sin_yaw;
            matrix.y_plane.y = sin_roll * sin_pitch * sin_yaw + cos_roll * cos_yaw;
            matrix.y_plane.z = -sin_roll * cos_pitch;
            matrix.y_plane.w = 0.f;

            matrix.z_plane.x = -( cos_roll * sin_pitch * cos_yaw + sin_roll * sin_yaw );
            matrix.z_plane.y = cos_yaw * sin_roll - cos_roll * sin_pitch * sin_yaw;
            matrix.z_plane.z = cos_roll * cos_pitch;
            matrix.z_plane.w = 0.f;

            matrix.w_plane.w = 1.f;
            return matrix;
        }

        f_matrix to_multiplication( f_matrix m_matrix ) const {
            f_matrix matrix{};

            matrix.w_plane.x = (
                this->w_plane.x * m_matrix.x_plane.x +
                this->w_plane.y * m_matrix.y_plane.x +
                this->w_plane.z * m_matrix.z_plane.x +
                this->w_plane.w * m_matrix.w_plane.x
                );

            matrix.w_plane.y = (
                this->w_plane.x * m_matrix.x_plane.y +
                this->w_plane.y * m_matrix.y_plane.y +
                this->w_plane.z * m_matrix.z_plane.y +
                this->w_plane.w * m_matrix.w_plane.y
                );

            matrix.w_plane.z = (
                this->w_plane.x * m_matrix.x_plane.z +
                this->w_plane.y * m_matrix.y_plane.z +
                this->w_plane.z * m_matrix.z_plane.z +
                this->w_plane.w * m_matrix.w_plane.z
                );

            matrix.w_plane.w = (
                this->w_plane.x * m_matrix.x_plane.w +
                this->w_plane.y * m_matrix.y_plane.w +
                this->w_plane.z * m_matrix.z_plane.w +
                this->w_plane.w * m_matrix.w_plane.w
                );

            return matrix;
        }
    };

    class f_transform {
    public:
        f_plane rotation;
        f_vector translation;
        char pad[ 0x4 ];
        f_vector scale;
        char pad1[ 0x4 ];

        f_transform( ) : rotation( ), translation( 0.f, 0.f, 0.f ), scale( 0.f, 0.f, 0.f ), pad( ), pad1( ) { }

        f_transform( const f_plane& rot, const f_vector& translation, const f_vector& scale ) {
            this->rotation = rot;
            this->translation = translation;

            this->pad[ 0x4 ] = 0;
            this->scale = scale;
            this->pad1[ 0x4 ] = 0;
        }

        void fix_scaler( ) {
            if ( this->scale.x == 0 ) this->scale.x = 1;
            if ( this->scale.y == 0 ) this->scale.y = 1;
            if ( this->scale.z == 0 ) this->scale.z = 1;
        }

        f_matrix to_matrix( ) {
            f_matrix matrix = {};

            auto x2 = this->rotation.x * 2;
            auto y2 = this->rotation.y * 2;
            auto z2 = this->rotation.z * 2;

            auto xx2 = this->rotation.x * x2;
            auto yy2 = this->rotation.y * y2;
            auto zz2 = this->rotation.z * z2;

            auto yz2 = this->rotation.y * z2;
            auto wx2 = this->rotation.w * x2;

            auto xy2 = this->rotation.x * y2;
            auto wz2 = this->rotation.w * z2;

            auto xz2 = this->rotation.x * z2;
            auto wy2 = this->rotation.w * y2;

            matrix.x_plane.x = ( 1.0 - ( yy2 + zz2 ) ) * this->scale.x;
            matrix.x_plane.y = ( xy2 + wz2 ) * this->scale.x;
            matrix.x_plane.z = ( xz2 - wy2 ) * this->scale.x;

            matrix.y_plane.x = ( xy2 - wz2 ) * this->scale.y;
            matrix.y_plane.y = ( 1.0 - ( xx2 + zz2 ) ) * this->scale.y;
            matrix.y_plane.z = ( yz2 + wx2 ) * this->scale.y;

            matrix.z_plane.x = ( xz2 + wy2 ) * this->scale.z;
            matrix.z_plane.y = ( yz2 - wx2 ) * this->scale.z;
            matrix.z_plane.z = ( 1.0 - ( xx2 + yy2 ) ) * this->scale.z;

            matrix.w_plane.x = this->translation.x;
            matrix.w_plane.y = this->translation.y;
            matrix.w_plane.z = this->translation.z;

            matrix.w_plane.w = 1.0;

            return matrix;
        }
    };

    struct fguid {
        int a;
        int b;
        int c;
        int d;
    };

    struct flinear_color {
    public:
        float r;
        float g;
        float b;
        float a;

        flinear_color with_alpha( float alpha ) const {
            return flinear_color{ r, g, b, alpha };
        }
    };
}